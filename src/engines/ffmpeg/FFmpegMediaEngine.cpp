#include "FFmpegMediaEngine.h"

#include "FFmpegCommandBuilder.h"
#include "FFprobe.h"
#include "core/FormatRegistry.h"
#include "hardware/HardwareAccelerationManager.h"

#include <QFileInfo>

using magnify::core::ConversionJob;
using magnify::core::FormatCategory;
using magnify::core::FormatRegistry;
using magnify::core::JobStatus;
using magnify::hardware::HardwareAccelerationManager;
using magnify::hardware::hardwareVendorFromString;

namespace magnify::engines::ffmpeg {

namespace {
// Applies the job's requested hardware backend (or CPU/libx264 if none is
// usable) to the builder. NVENC/AMF/QSV don't share libx264's CRF semantics,
// so hardware encodes use a bitrate target instead.
void applyVideoEncoder(FFmpegCommandBuilder &builder, const magnify::core::JobParameters &params) {
    const auto vendor = hardwareVendorFromString(params.value(QStringLiteral("hardwareBackend"), "auto").toString());
    const QString encoder = HardwareAccelerationManager::instance().encoderFor(vendor, QStringLiteral("h264"));

    if (encoder == QStringLiteral("libx264") || encoder == QStringLiteral("libx265")) {
        builder.setVideoCodec(params.value(QStringLiteral("videoCodec"), encoder).toString());
        builder.setCrf(params.value(QStringLiteral("crf"), 23).toInt());
    } else {
        builder.setHardwareEncoder(encoder);
        builder.setVideoBitrate(params.value(QStringLiteral("videoBitrate"), "6000k").toString());
    }

    // Presets (YouTube 1080p, Discord, WhatsApp, ...) request a target
    // resolution this way; a plain conversion leaves the source resolution
    // untouched since neither key is present.
    if (params.contains(QStringLiteral("width")) && params.contains(QStringLiteral("height"))) {
        builder.setResolution(params.value(QStringLiteral("width")).toInt(),
                               params.value(QStringLiteral("height")).toInt());
    }
}
} // namespace

FFmpegMediaEngine::FFmpegMediaEngine(QObject *parent) : IMediaEngine(parent) {
}

MediaProbeResult FFmpegMediaEngine::probe(const QString &filePath) {
    return FFprobe::probe(filePath);
}

QStringList FFmpegMediaEngine::buildArgsForJob(ConversionJob *job, const MediaProbeResult &probeResult) {
    FFmpegCommandBuilder builder;
    builder.setInput(job->inputPath()).setOutput(job->outputPath()).setOverwrite(true).enableProgressReporting();

    const QString targetExt = job->targetFormat().toLower();
    const FormatCategory targetCategory = FormatRegistry::instance().categoryOf(targetExt);
    const auto &params = job->parameters();

    const bool wantsTrim =
        params.contains(QStringLiteral("trimStart")) || params.contains(QStringLiteral("trimEnd"));
    if (wantsTrim) {
        const double start = params.value(QStringLiteral("trimStart"), 0.0).toDouble();
        if (params.contains(QStringLiteral("trimEnd"))) {
            builder.setTrim(start, params.value(QStringLiteral("trimEnd")).toDouble());
        } else {
            builder.setTrim(start);
        }
    }

    if (targetCategory == FormatCategory::Audio) {
        // Any container/codec -> pure audio output: drop video entirely.
        builder.dropVideoStream();

        if (targetExt == QStringLiteral("mp3")) {
            builder.setAudioCodec(QStringLiteral("libmp3lame"));
            // A preset asking for an explicit bitrate (e.g. "MP3 320 kbps")
            // wants CBR; otherwise fall back to VBR quality (-q:a).
            if (params.contains(QStringLiteral("audioBitrate"))) {
                builder.setAudioBitrate(params.value(QStringLiteral("audioBitrate")).toString());
            } else {
                builder.setAudioQuality(params.value(QStringLiteral("audioQuality"), 2).toInt());
            }
        } else if (targetExt == QStringLiteral("wav")) {
            builder.setAudioCodec(QStringLiteral("pcm_s16le"));
        } else if (targetExt == QStringLiteral("flac")) {
            builder.setAudioCodec(QStringLiteral("flac"));
        } else if (targetExt == QStringLiteral("aac") || targetExt == QStringLiteral("m4a")) {
            builder.setAudioCodec(QStringLiteral("aac"));
            builder.setAudioBitrate(params.value(QStringLiteral("audioBitrate"), "192k").toString());
        } else if (targetExt == QStringLiteral("ogg")) {
            builder.setAudioCodec(QStringLiteral("libvorbis"));
        } else {
            builder.setAudioCodec(QStringLiteral("copy"));
        }

        if (params.contains(QStringLiteral("sampleRate"))) {
            builder.setSampleRate(params.value(QStringLiteral("sampleRate")).toInt());
        }
        if (params.contains(QStringLiteral("channels"))) {
            builder.setAudioChannels(params.value(QStringLiteral("channels")).toInt());
        }
        return builder.build();
    }

    if (targetCategory == FormatCategory::Image) {
        // Still-image conversion (PNG/JPEG/WebP/AVIF/BMP/TIFF/GIF/...). FFmpeg
        // selects an appropriate encoder from the output extension by default;
        // we only need to steer quality for the lossy formats users care about.
        if (targetExt == QStringLiteral("jpg") || targetExt == QStringLiteral("jpeg")) {
            builder.addExtraArgs({QStringLiteral("-q:v"),
                                   QString::number(params.value(QStringLiteral("jpegQuality"), 2).toInt())});
        } else if (targetExt == QStringLiteral("webp")) {
            builder.addExtraArgs(
                {QStringLiteral("-quality"), QString::number(params.value(QStringLiteral("quality"), 85).toInt())});
        } else if (targetExt == QStringLiteral("avif")) {
            builder.setVideoCodec(QStringLiteral("libaom-av1"));
            builder.addExtraArgs({QStringLiteral("-crf"),
                                   QString::number(params.value(QStringLiteral("crf"), 30).toInt()),
                                   QStringLiteral("-still-picture"), QStringLiteral("1")});
        }
        return builder.build();
    }

    if (targetCategory == FormatCategory::Video) {
        const QString sourceExt = job->sourceFormat().toLower();
        const bool sourceIsH264 = probeResult.videoCodec == QStringLiteral("h264");
        const bool sourceIsAac = probeResult.audioCodec == QStringLiteral("aac");

        if (targetExt == QStringLiteral("mpeg") || targetExt == QStringLiteral("mpg")) {
            // MPEG-1/2 does not support H.264/AAC, so this path always re-encodes.
            builder.setVideoCodec(QStringLiteral("mpeg2video"));
            builder.setVideoBitrate(params.value(QStringLiteral("videoBitrate"), "6000k").toString());
            builder.setAudioCodec(QStringLiteral("mp2"));
            builder.setAudioBitrate(QStringLiteral("192k"));
            return builder.build();
        }

        if (targetExt == QStringLiteral("mp4") || targetExt == QStringLiteral("mkv") ||
            targetExt == QStringLiteral("mov")) {
            // Remux without re-encoding whenever the codecs are already compatible
            // with the destination container (e.g. MKV H.264/AAC -> MP4). A
            // stream copy can't apply a resolution change, so any request for
            // one (a preset like Discord/WhatsApp, or explicit --width/--height)
            // forces the re-encode path even when the codecs already match.
            const bool wantsResize =
                params.contains(QStringLiteral("width")) || params.contains(QStringLiteral("height"));
            // Trimming with a stream copy would snap to the nearest keyframe
            // instead of the exact requested time, so it always forces a
            // re-encode.
            const bool canStreamCopy = !params.value(QStringLiteral("forceReencode"), false).toBool() &&
                                        !wantsResize && !wantsTrim && sourceExt != targetExt && sourceIsH264 &&
                                        sourceIsAac;
            if (canStreamCopy) {
                builder.copyVideoStream().copyAudioStream();
            } else {
                applyVideoEncoder(builder, params);
                builder.setAudioCodec(QStringLiteral("aac"));
                builder.setAudioBitrate(params.value(QStringLiteral("audioBitrate"), "192k").toString());
            }
            return builder.build();
        }

        // Generic fallback: re-encode to H.264/AAC for any other target container.
        applyVideoEncoder(builder, params);
        builder.setAudioCodec(QStringLiteral("aac"));
        return builder.build();
    }

    // Unknown target category: pass through as a plain remux attempt.
    builder.copyVideoStream().copyAudioStream();
    return builder.build();
}

void FFmpegMediaEngine::startConversion(ConversionJob *job) {
    const QUuid jobId = job->id();
    if (m_runningJobs.contains(jobId)) {
        return;
    }

    job->setStatus(JobStatus::Preparing);
    const MediaProbeResult probeResult = FFprobe::probe(job->inputPath());
    if (!probeResult.valid) {
        job->setErrorMessage(probeResult.errorMessage);
        job->setStatus(JobStatus::Failed);
        emit jobFinished(jobId, false, probeResult.errorMessage);
        return;
    }

    const QStringList args = buildArgsForJob(job, probeResult);

    auto *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::SeparateChannels);

    RunningJob running;
    running.process = process;
    running.job = job;
    running.totalDurationSeconds = probeResult.durationSeconds;
    running.wallClock.start();
    m_runningJobs.insert(jobId, running);

    connect(process, &QProcess::readyReadStandardOutput, this, [this, jobId]() {
        handleProgressData(jobId);
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, jobId]() {
        if (auto it = m_runningJobs.find(jobId); it != m_runningJobs.end()) {
            it->stderrBuffer += it->process->readAllStandardError();
        }
    });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, jobId](int exitCode, QProcess::ExitStatus exitStatus) {
                handleProcessFinished(jobId, exitCode, exitStatus);
            });
    // Without this, a process that fails to even start (ffmpeg missing from
    // PATH, permissions, ...) never emits finished(), and the job — and any
    // caller blocked on its statusChanged signal — hangs forever instead of
    // failing.
    connect(process, &QProcess::errorOccurred, this, [this, jobId](QProcess::ProcessError error) {
        auto it = m_runningJobs.find(jobId);
        if (error != QProcess::FailedToStart || it == m_runningJobs.end()) {
            return;
        }
        RunningJob running = it.value();
        m_runningJobs.erase(it);
        const QString errorText = QStringLiteral("Could not start ffmpeg: %1").arg(running.process->errorString());
        running.job->setErrorMessage(errorText);
        running.job->setStatus(JobStatus::Failed);
        emit jobFinished(jobId, false, errorText);
        running.process->deleteLater();
    });

    job->setStatus(JobStatus::Running);
    process->start(QStringLiteral("ffmpeg"), args);
}

void FFmpegMediaEngine::handleProgressData(const QUuid &jobId) {
    auto it = m_runningJobs.find(jobId);
    if (it == m_runningJobs.end()) {
        return;
    }
    RunningJob &running = it.value();
    running.progressBuffer += running.process->readAllStandardOutput();

    // ffmpeg -progress writes one key=value pair per line, terminated by
    // "progress=continue" or "progress=end" for each reporting interval.
    int newlineIndex;
    double outTimeSeconds = -1.0;
    double speed = 0.0;

    while ((newlineIndex = running.progressBuffer.indexOf('\n')) != -1) {
        const QByteArray line = running.progressBuffer.left(newlineIndex).trimmed();
        running.progressBuffer.remove(0, newlineIndex + 1);

        const int eq = line.indexOf('=');
        if (eq == -1) {
            continue;
        }
        const QByteArray key = line.left(eq);
        const QByteArray value = line.mid(eq + 1);

        if (key == "out_time_ms" || key == "out_time_us") {
            // Despite its name, ffmpeg's "out_time_ms" field is actually
            // microseconds (a long-standing, documented ffmpeg quirk kept for
            // backwards compatibility); "out_time_us" is the same value under
            // its correctly spelled name. Both divide by 1e6, not 1e3.
            outTimeSeconds = value.toDouble() / 1'000'000.0;
        } else if (key == "speed") {
            QByteArray v = value;
            v.replace("x", "");
            speed = v.trimmed().toDouble();
        } else if (key == "progress") {
            const bool finished = (value == "end");
            if (outTimeSeconds >= 0.0 && running.totalDurationSeconds > 0.0) {
                const int percent = finished
                    ? 100
                    : qBound(0, static_cast<int>((outTimeSeconds / running.totalDurationSeconds) * 100.0), 100);
                running.job->setProgressPercent(percent);
                running.job->setSpeedFactor(speed);

                qint64 eta = -1;
                if (speed > 0.0) {
                    const double remainingSeconds = running.totalDurationSeconds - outTimeSeconds;
                    eta = static_cast<qint64>(remainingSeconds / speed);
                }
                running.job->setEtaSeconds(eta);
                emit jobProgress(jobId, percent, speed, eta);
            }
        }
    }
}

void FFmpegMediaEngine::handleProcessFinished(const QUuid &jobId, int exitCode, QProcess::ExitStatus exitStatus) {
    auto it = m_runningJobs.find(jobId);
    if (it == m_runningJobs.end()) {
        return;
    }
    RunningJob running = it.value();
    m_runningJobs.erase(it);

    const bool success = (exitStatus == QProcess::NormalExit) && (exitCode == 0) &&
                          QFileInfo::exists(running.job->outputPath());

    if (success) {
        running.job->setProgressPercent(100);
        running.job->setStatus(JobStatus::Completed);
        emit jobFinished(jobId, true, QString());
    } else {
        const QString errorText = QString::fromUtf8(running.stderrBuffer).trimmed();
        running.job->setErrorMessage(errorText);
        running.job->setStatus(JobStatus::Failed);
        emit jobFinished(jobId, false, errorText);
    }

    running.process->deleteLater();
}

void FFmpegMediaEngine::cancelConversion(const QUuid &jobId) {
    auto it = m_runningJobs.find(jobId);
    if (it == m_runningJobs.end()) {
        return;
    }
    it->job->setStatus(JobStatus::Cancelled);
    it->process->kill();
}

} // namespace magnify::engines::ffmpeg
