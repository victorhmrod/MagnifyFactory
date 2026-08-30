// Standalone smoke test: drives FFmpegMediaEngine::buildAudioEditArgs()
// through a real multi-clip edit (trim each clip, concatenate, speed up,
// volume/fade/normalize) and verifies the output for real via ffprobe
// (duration) and ffmpeg's volumedetect filter (fade envelope — quiet at the
// very start/end, loud in the middle). Not part of ctest — shells out to
// real ffmpeg/ffprobe and needs synthetic clips; run manually during audio
// editor verification.
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>
#include <QDebug>

#include "core/ConversionJob.h"
#include "core/JobManager.h"
#include "engines/ffmpeg/FFmpegMediaEngine.h"

using magnify::core::ConversionJob;
using magnify::core::JobManager;
using magnify::core::JobStatus;

namespace {
// Runs ffmpeg's volumedetect filter over [start, start+duration) of `path`
// and returns max_volume in dB (a very negative number, e.g. -90, means
// near-silence). Returns 0.0 (i.e. "loud") on parse failure so a bug fails
// the fade check as "not quiet enough" rather than silently passing.
double maxVolumeDb(const QString &path, double start, double duration) {
    QProcess proc;
    proc.start("ffmpeg", {"-ss", QString::number(start, 'f', 3), "-t", QString::number(duration, 'f', 3), "-i", path,
                          "-af", "volumedetect", "-f", "null", "-"});
    proc.waitForFinished(10000);
    const QString stderrText = QString::fromUtf8(proc.readAllStandardError());
    const QRegularExpression re(QStringLiteral("max_volume:\\s*(-?[0-9.]+) dB"));
    const auto match = re.match(stderrText);
    return match.hasMatch() ? match.captured(1).toDouble() : 0.0;
}
} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    const QString dir = QDir::currentPath();

    // Two 4s synthetic sine-wave clips at full volume, distinguishable by
    // frequency (not that this test checks frequency — just needs two real,
    // loud, non-empty audio sources).
    const QString clip1 = dir + "/audio_clip1.wav";
    const QString clip2 = dir + "/audio_clip2.wav";
    QProcess gen1;
    gen1.start("ffmpeg", {"-y", "-f", "lavfi", "-i", "sine=frequency=440:duration=4", "-loglevel", "error", clip1});
    gen1.waitForFinished(20000);
    QProcess gen2;
    gen2.start("ffmpeg", {"-y", "-f", "lavfi", "-i", "sine=frequency=880:duration=4", "-loglevel", "error", clip2});
    gen2.waitForFinished(20000);
    if (!QFileInfo::exists(clip1) || !QFileInfo::exists(clip2)) {
        qWarning() << "FAILED: could not generate synthetic clips";
        return 1;
    }

    // Edit: clip1 trimmed to [1s,3s) (2s) + clip2 trimmed to [0s,2s) (2s) =
    // 4s concatenated, sped up 2x -> expect a ~2s output, with a 0.3s fade
    // in and 0.3s fade out, a volume cut, and loudness normalization.
    const QString outPath = dir + "/audio_edit_output.mp3";
    auto job = std::make_unique<ConversionJob>(clip1, outPath);
    job->setExtraInputPaths({clip2});
    job->setSourceFormat("wav");
    job->setTargetFormat("mp3");
    job->setEngineName("FFmpeg");
    job->setParameters({
        {"operation", "audioEdit"},
        {"clipTrims", QVariantList{QVariantList{1.0, 3.0}, QVariantList{0.0, 2.0}}},
        {"speed", 2.0},
        {"volumeDb", -6.0},
        {"fadeInSeconds", 0.3},
        {"fadeOutSeconds", 0.3},
        {"normalize", true},
    });

    magnify::engines::ffmpeg::FFmpegMediaEngine engine;
    JobManager manager;
    manager.registerEngine(&engine);
    ConversionJob *raw = manager.addJob(std::move(job));

    QEventLoop loop;
    bool finished = false, ok = false;
    QObject::connect(raw, &ConversionJob::statusChanged, &loop, [&](JobStatus status) {
        if (status == JobStatus::Completed || status == JobStatus::Failed || status == JobStatus::Cancelled) {
            finished = true;
            ok = (status == JobStatus::Completed);
            loop.quit();
        }
    });
    QTimer::singleShot(60000, &loop, &QEventLoop::quit);
    manager.startQueue();
    loop.exec();

    if (!finished || !ok) {
        qWarning() << "FAILED: audio edit job did not complete:" << raw->errorMessage();
        return 1;
    }

    bool allOk = true;

    QProcess probe;
    probe.start("ffprobe", {"-v", "error", "-show_entries", "format=duration", "-of",
                             "default=noprint_wrappers=1:nokey=1", outPath});
    probe.waitForFinished(10000);
    const double duration = QString::fromUtf8(probe.readAllStandardOutput()).trimmed().toDouble();
    // (2s + 2s trimmed) / 2x speed = 2s, with tolerance for encoder rounding.
    if (qAbs(duration - 2.0) > 0.5) {
        qWarning() << "FAILED: expected ~2s output, got" << duration << "s";
        allOk = false;
    } else {
        qInfo() << "OK: edited output duration is" << duration << "s (expected ~2s)";
    }

    // Fade envelope: near-silent right at the start and end, clearly audible
    // in the middle — this is only true if the fade-in/out filters actually
    // ran (a bug that skipped them would leave the whole clip at a flat,
    // audible volume throughout, including the very first/last samples).
    const double startVolume = maxVolumeDb(outPath, 0.0, 0.05);
    const double midVolume = maxVolumeDb(outPath, duration / 2.0 - 0.05, 0.1);
    const double endVolume = maxVolumeDb(outPath, qMax(0.0, duration - 0.05), 0.05);

    if (startVolume > -20.0) {
        qWarning() << "FAILED: start of clip should be near-silent (fade-in), max_volume =" << startVolume << "dB";
        allOk = false;
    } else {
        qInfo() << "OK: start of clip is quiet (fade-in), max_volume =" << startVolume << "dB";
    }
    if (midVolume < -20.0) {
        qWarning() << "FAILED: middle of clip should be audible, max_volume =" << midVolume << "dB";
        allOk = false;
    } else {
        qInfo() << "OK: middle of clip is audible, max_volume =" << midVolume << "dB";
    }
    if (endVolume > -20.0) {
        qWarning() << "FAILED: end of clip should be near-silent (fade-out), max_volume =" << endVolume << "dB";
        allOk = false;
    } else {
        qInfo() << "OK: end of clip is quiet (fade-out), max_volume =" << endVolume << "dB";
    }

    qInfo() << (allOk ? "ALL AUDIO EDITOR CHECKS PASSED" : "SOME AUDIO EDITOR CHECKS FAILED");
    return allOk ? 0 : 1;
}
