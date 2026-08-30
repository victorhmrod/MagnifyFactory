#include "FFmpegMediaEngine.h"

#include "FFmpegCommandBuilder.h"
#include "FFprobe.h"
#include "core/FormatRegistry.h"
#include "core/HostProcess.h"
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

    // Extracting a subtitle track is a fundamentally different ffmpeg
    // invocation (map the subtitle stream, drop everything else) than any
    // convert/trim path below, so it's handled as its own early return.
    if (params.value(QStringLiteral("operation")).toString() == QStringLiteral("extractSubtitles")) {
        builder.addExtraArgs({QStringLiteral("-map"), QStringLiteral("0:s:0"), QStringLiteral("-c:s"),
                               QStringLiteral("srt")});
        return builder.build();
    }

    if (params.contains(QStringLiteral("rotate"))) {
        switch (params.value(QStringLiteral("rotate")).toInt()) {
            case 90: builder.addVideoFilter(QStringLiteral("transpose=1")); break;
            case -90:
            case 270: builder.addVideoFilter(QStringLiteral("transpose=2")); break;
            case 180: builder.addVideoFilter(QStringLiteral("transpose=1,transpose=1")); break;
            default: break;
        }
    }

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
        const FormatCategory sourceCategory = FormatRegistry::instance().categoryOf(job->sourceFormat().toLower());
        if (sourceCategory == FormatCategory::Video) {
            // A video source has many frames; without this the image2 muxer
            // rejects the output ("does not contain an image sequence
            // pattern") instead of just writing a single still. Grabs the
            // first frame.
            builder.addExtraArgs({QStringLiteral("-frames:v"), QStringLiteral("1")});
        }
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

namespace {
// Escapes a string for safe use inside an ffmpeg drawtext filter's text=
// value (single-quoted within the filter graph): backslash and single quote
// need backslash-escaping, and since the whole filtergraph is itself
// colon/comma-delimited, those also need escaping so a title containing
// punctuation doesn't get parsed as extra drawtext options or filter
// separators.
QString escapeDrawtext(const QString &text) {
    QString escaped = text;
    escaped.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    escaped.replace(QStringLiteral("'"), QStringLiteral("\\'"));
    escaped.replace(QStringLiteral(":"), QStringLiteral("\\:"));
    escaped.replace(QStringLiteral(","), QStringLiteral("\\,"));
    return escaped;
}

// atempo only accepts 0.5-2.0 per instance; anything outside that range
// needs to be expressed as a chain of instances multiplying to the
// requested factor. The video editor's UI keeps speed within 0.5x-2.0x
// (see VideoEditorDialog), so this is a single instance in practice, but
// the helper is written to degrade sanely if that ever changes.
QString atempoChain(double speed) {
    QStringList stages;
    double remaining = speed;
    while (remaining > 2.0) {
        stages << QStringLiteral("atempo=2.0");
        remaining /= 2.0;
    }
    while (remaining < 0.5) {
        stages << QStringLiteral("atempo=0.5");
        remaining /= 0.5;
    }
    stages << QStringLiteral("atempo=%1").arg(remaining, 0, 'f', 4);
    return stages.join(',');
}

// drawtext needs a real font file to render text; without one it falls back
// to fontconfig, which isn't reliably configured on Windows (ffmpeg builds
// there commonly lack a working fontconfig setup, producing a hard
// "Cannot load default config file" failure instead of just rendering with
// a system default the way it does on most Linux desktops). Pointing it at
// a known-present font sidesteps fontconfig entirely, on every platform.
QString defaultFontFile() {
#if defined(Q_OS_WIN)
    return QStringLiteral("C:/Windows/Fonts/arial.ttf");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("/System/Library/Fonts/Helvetica.ttc");
#else
    const QStringList candidates{
        QStringLiteral("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"),
        QStringLiteral("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"),
        QStringLiteral("/usr/share/fonts/TTF/DejaVuSans.ttf"),
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return QString(); // fontconfig is the norm on Linux; let it pick a default
#endif
}

void drawtextPositionExpr(const QString &position, QString *xExpr, QString *yExpr) {
    if (position == QStringLiteral("top-left")) {
        *xExpr = QStringLiteral("20");
        *yExpr = QStringLiteral("20");
    } else if (position == QStringLiteral("top-right")) {
        *xExpr = QStringLiteral("w-tw-20");
        *yExpr = QStringLiteral("20");
    } else if (position == QStringLiteral("bottom-left")) {
        *xExpr = QStringLiteral("20");
        *yExpr = QStringLiteral("h-th-20");
    } else if (position == QStringLiteral("center")) {
        *xExpr = QStringLiteral("(w-tw)/2");
        *yExpr = QStringLiteral("(h-th)/2");
    } else {
        // bottom-right, and the default for anything unrecognized
        *xExpr = QStringLiteral("w-tw-20");
        *yExpr = QStringLiteral("h-th-20");
    }
}
} // namespace

QStringList FFmpegMediaEngine::buildVideoEditArgs(ConversionJob *job) {
    QStringList clips;
    clips << job->inputPath();
    clips << job->extraInputPaths();

    const auto &params = job->parameters();
    const QVariantList clipTrims = params.value(QStringLiteral("clipTrims")).toList();

    QStringList args{QStringLiteral("-y"), QStringLiteral("-hide_banner")};
    for (const QString &clip : clips) {
        args << QStringLiteral("-i") << clip;
    }

    QStringList filterParts;
    QStringList vLabels, aLabels;
    for (int i = 0; i < clips.size(); ++i) {
        const QVariantList trim = i < clipTrims.size() ? clipTrims.at(i).toList() : QVariantList();
        const double start = trim.size() > 0 ? trim.at(0).toDouble() : 0.0;
        const bool hasEnd = trim.size() > 1 && trim.at(1).toDouble() > 0.0;
        const double end = hasEnd ? trim.at(1).toDouble() : 0.0;

        QString vTrim = QStringLiteral("trim=start=%1").arg(start, 0, 'f', 3);
        if (hasEnd) {
            vTrim += QStringLiteral(":end=%1").arg(end, 0, 'f', 3);
        }
        filterParts << QStringLiteral("[%1:v]%2,setpts=PTS-STARTPTS[v%1]").arg(i).arg(vTrim);

        QString aTrim = QStringLiteral("atrim=start=%1").arg(start, 0, 'f', 3);
        if (hasEnd) {
            aTrim += QStringLiteral(":end=%1").arg(end, 0, 'f', 3);
        }
        filterParts << QStringLiteral("[%1:a]%2,asetpts=PTS-STARTPTS[a%1]").arg(i).arg(aTrim);

        vLabels << QStringLiteral("[v%1]").arg(i);
        aLabels << QStringLiteral("[a%1]").arg(i);
    }

    // concat interleaves inputs as v0,a0,v1,a1,...
    QStringList concatInputs;
    for (int i = 0; i < clips.size(); ++i) {
        concatInputs << vLabels.at(i) << aLabels.at(i);
    }
    filterParts << QStringLiteral("%1concat=n=%2:v=1:a=1[cv][ca]").arg(concatInputs.join(QString())).arg(clips.size());

    QString videoLabel = QStringLiteral("cv");
    QString audioLabel = QStringLiteral("ca");

    const double brightness = params.value(QStringLiteral("brightness"), 0.0).toDouble();
    const double contrast = params.value(QStringLiteral("contrast"), 1.0).toDouble();
    const double saturation = params.value(QStringLiteral("saturation"), 1.0).toDouble();
    if (brightness != 0.0 || contrast != 1.0 || saturation != 1.0) {
        filterParts << QStringLiteral("[%1]eq=brightness=%2:contrast=%3:saturation=%4[eqv]")
                            .arg(videoLabel)
                            .arg(brightness, 0, 'f', 3)
                            .arg(contrast, 0, 'f', 3)
                            .arg(saturation, 0, 'f', 3);
        videoLabel = QStringLiteral("eqv");
    }

    const double speed = params.value(QStringLiteral("speed"), 1.0).toDouble();
    if (speed > 0.0 && speed != 1.0) {
        filterParts << QStringLiteral("[%1]setpts=PTS/%2[spv]").arg(videoLabel).arg(speed, 0, 'f', 4);
        videoLabel = QStringLiteral("spv");
        filterParts << QStringLiteral("[%1]%2[spa]").arg(audioLabel, atempoChain(speed));
        audioLabel = QStringLiteral("spa");
    }

    const QString overlayText = params.value(QStringLiteral("overlayText")).toString();
    if (!overlayText.isEmpty()) {
        QString xExpr, yExpr;
        drawtextPositionExpr(params.value(QStringLiteral("overlayPosition"), "bottom-right").toString(), &xExpr,
                              &yExpr);
        const int fontSize = params.value(QStringLiteral("overlayFontSize"), 32).toInt();
        const QString fontFile = defaultFontFile();
        // A colon in the font path (e.g. "C:/Windows/...") would otherwise
        // be parsed as a drawtext key=value separator.
        const QString fontFileArg =
            fontFile.isEmpty() ? QString()
                                : QStringLiteral(":fontfile='%1'").arg(QString(fontFile).replace(':', "\\:"));
        filterParts << QStringLiteral("[%1]drawtext=text='%2':x=%3:y=%4:fontsize=%5:fontcolor=white:"
                                       "borderw=2:bordercolor=black@0.7%6[txtv]")
                            .arg(videoLabel, escapeDrawtext(overlayText), xExpr, yExpr)
                            .arg(fontSize)
                            .arg(fontFileArg);
        videoLabel = QStringLiteral("txtv");
    }

    args << QStringLiteral("-filter_complex") << filterParts.join(';');
    args << QStringLiteral("-map") << QStringLiteral("[%1]").arg(videoLabel);
    args << QStringLiteral("-map") << QStringLiteral("[%1]").arg(audioLabel);
    args << QStringLiteral("-c:v") << QStringLiteral("libx264") << QStringLiteral("-crf") << QStringLiteral("20");
    args << QStringLiteral("-c:a") << QStringLiteral("aac");
    args << QStringLiteral("-progress") << QStringLiteral("pipe:1") << QStringLiteral("-nostats");
    args << job->outputPath();
    return args;
}

QStringList FFmpegMediaEngine::buildImageEditArgs(ConversionJob *job) {
    FFmpegCommandBuilder builder;
    builder.setInput(job->inputPath()).setOutput(job->outputPath()).setOverwrite(true);

    const auto &params = job->parameters();

    const QVariantList crop = params.value(QStringLiteral("crop")).toList();
    if (crop.size() == 4) {
        builder.addVideoFilter(QStringLiteral("crop=%1:%2:%3:%4")
                                    .arg(crop.at(2).toInt())
                                    .arg(crop.at(3).toInt())
                                    .arg(crop.at(0).toInt())
                                    .arg(crop.at(1).toInt()));
    }

    if (params.contains(QStringLiteral("resizeWidth")) && params.contains(QStringLiteral("resizeHeight"))) {
        builder.addVideoFilter(QStringLiteral("scale=%1:%2:flags=lanczos")
                                    .arg(params.value(QStringLiteral("resizeWidth")).toInt())
                                    .arg(params.value(QStringLiteral("resizeHeight")).toInt()));
    }

    const double brightness = params.value(QStringLiteral("brightness"), 0.0).toDouble();
    const double contrast = params.value(QStringLiteral("contrast"), 1.0).toDouble();
    const double saturation = params.value(QStringLiteral("saturation"), 1.0).toDouble();
    if (brightness != 0.0 || contrast != 1.0 || saturation != 1.0) {
        builder.addVideoFilter(QStringLiteral("eq=brightness=%1:contrast=%2:saturation=%3")
                                    .arg(brightness, 0, 'f', 3)
                                    .arg(contrast, 0, 'f', 3)
                                    .arg(saturation, 0, 'f', 3));
    }

    const double blur = params.value(QStringLiteral("blur"), 0.0).toDouble();
    if (blur > 0.0) {
        builder.addVideoFilter(QStringLiteral("gblur=sigma=%1").arg(blur, 0, 'f', 2));
    }

    const QString overlayText = params.value(QStringLiteral("overlayText")).toString();
    if (!overlayText.isEmpty()) {
        QString xExpr, yExpr;
        drawtextPositionExpr(params.value(QStringLiteral("overlayPosition"), "bottom-right").toString(), &xExpr,
                              &yExpr);
        const int fontSize = params.value(QStringLiteral("overlayFontSize"), 32).toInt();
        const QString fontFile = defaultFontFile();
        const QString fontFileArg =
            fontFile.isEmpty() ? QString()
                                : QStringLiteral(":fontfile='%1'").arg(QString(fontFile).replace(':', "\\:"));
        builder.addVideoFilter(QStringLiteral("drawtext=text='%1':x=%2:y=%3:fontsize=%4:fontcolor=white:"
                                               "borderw=2:bordercolor=black@0.7%5")
                                    .arg(escapeDrawtext(overlayText), xExpr, yExpr)
                                    .arg(fontSize)
                                    .arg(fontFileArg));
    }

    const QString targetExt = job->targetFormat().toLower();
    if (targetExt == QStringLiteral("jpg") || targetExt == QStringLiteral("jpeg")) {
        builder.addExtraArgs(
            {QStringLiteral("-q:v"), QString::number(params.value(QStringLiteral("jpegQuality"), 2).toInt())});
    } else if (targetExt == QStringLiteral("webp")) {
        builder.addExtraArgs(
            {QStringLiteral("-quality"), QString::number(params.value(QStringLiteral("quality"), 90).toInt())});
    }

    return builder.build();
}

namespace {
QString audioCodecForExt(const QString &targetExt) {
    if (targetExt == QStringLiteral("mp3")) {
        return QStringLiteral("libmp3lame");
    }
    if (targetExt == QStringLiteral("wav")) {
        return QStringLiteral("pcm_s16le");
    }
    if (targetExt == QStringLiteral("flac")) {
        return QStringLiteral("flac");
    }
    if (targetExt == QStringLiteral("ogg")) {
        return QStringLiteral("libvorbis");
    }
    // aac, m4a, and anything unrecognized.
    return QStringLiteral("aac");
}
} // namespace

QStringList FFmpegMediaEngine::buildAudioEditArgs(ConversionJob *job) {
    QStringList clips;
    clips << job->inputPath();
    clips << job->extraInputPaths();

    const auto &params = job->parameters();
    const QVariantList clipTrims = params.value(QStringLiteral("clipTrims")).toList();

    QStringList args{QStringLiteral("-y"), QStringLiteral("-hide_banner")};
    for (const QString &clip : clips) {
        args << QStringLiteral("-i") << clip;
    }

    QStringList filterParts;
    QStringList aLabels;
    for (int i = 0; i < clips.size(); ++i) {
        const QVariantList trim = i < clipTrims.size() ? clipTrims.at(i).toList() : QVariantList();
        const double start = trim.size() > 0 ? trim.at(0).toDouble() : 0.0;
        const bool hasEnd = trim.size() > 1 && trim.at(1).toDouble() > 0.0;
        const double end = hasEnd ? trim.at(1).toDouble() : 0.0;

        QString aTrim = QStringLiteral("atrim=start=%1").arg(start, 0, 'f', 3);
        if (hasEnd) {
            aTrim += QStringLiteral(":end=%1").arg(end, 0, 'f', 3);
        }
        filterParts << QStringLiteral("[%1:a]%2,asetpts=PTS-STARTPTS[a%1]").arg(i).arg(aTrim);
        aLabels << QStringLiteral("[a%1]").arg(i);
    }

    filterParts << QStringLiteral("%1concat=n=%2:v=0:a=1[ca]").arg(aLabels.join(QString())).arg(clips.size());
    QString audioLabel = QStringLiteral("ca");

    const double volumeDb = params.value(QStringLiteral("volumeDb"), 0.0).toDouble();
    if (volumeDb != 0.0) {
        filterParts << QStringLiteral("[%1]volume=%2dB[vola]").arg(audioLabel).arg(volumeDb, 0, 'f', 2);
        audioLabel = QStringLiteral("vola");
    }

    const double speed = params.value(QStringLiteral("speed"), 1.0).toDouble();
    if (speed > 0.0 && speed != 1.0) {
        filterParts << QStringLiteral("[%1]%2[spa]").arg(audioLabel, atempoChain(speed));
        audioLabel = QStringLiteral("spa");
    }

    if (params.value(QStringLiteral("normalize"), false).toBool()) {
        filterParts << QStringLiteral("[%1]loudnorm=I=-16:LRA=11:TP=-1.5[norma]").arg(audioLabel);
        audioLabel = QStringLiteral("norma");
    }

    const double fadeInSeconds = params.value(QStringLiteral("fadeInSeconds"), 0.0).toDouble();
    if (fadeInSeconds > 0.0) {
        filterParts << QStringLiteral("[%1]afade=t=in:st=0:d=%2[fadein]").arg(audioLabel).arg(fadeInSeconds, 0, 'f', 3);
        audioLabel = QStringLiteral("fadein");
    }

    // Fading out needs the fade to start relative to the END of the stream,
    // which isn't known here without a second probing pass — the standard
    // ffmpeg workaround is to reverse, fade *in* over the desired duration,
    // then reverse back, which fades out the true end regardless of length.
    const double fadeOutSeconds = params.value(QStringLiteral("fadeOutSeconds"), 0.0).toDouble();
    if (fadeOutSeconds > 0.0) {
        filterParts << QStringLiteral("[%1]areverse,afade=t=in:st=0:d=%2,areverse[fadeout]")
                            .arg(audioLabel)
                            .arg(fadeOutSeconds, 0, 'f', 3);
        audioLabel = QStringLiteral("fadeout");
    }

    args << QStringLiteral("-filter_complex") << filterParts.join(';');
    args << QStringLiteral("-map") << QStringLiteral("[%1]").arg(audioLabel);
    args << QStringLiteral("-c:a") << audioCodecForExt(job->targetFormat().toLower());
    args << QStringLiteral("-progress") << QStringLiteral("pipe:1") << QStringLiteral("-nostats");
    args << job->outputPath();
    return args;
}

void FFmpegMediaEngine::startConversion(ConversionJob *job) {
    const QUuid jobId = job->id();
    if (m_runningJobs.contains(jobId)) {
        return;
    }

    job->setStatus(JobStatus::Preparing);

    const bool isVideoEdit =
        job->parameters().value(QStringLiteral("operation")).toString() == QStringLiteral("videoEdit");

    QStringList args;
    double totalDurationSeconds = 0.0;

    if (isVideoEdit) {
        // Multiple inputs, no single probeResult to drive progress% off of —
        // sum every clip's real duration instead (trim points aren't
        // subtracted; close enough for an ETA, not worth a second pass).
        QStringList clips{job->inputPath()};
        clips << job->extraInputPaths();
        for (const QString &clip : clips) {
            const MediaProbeResult clipProbe = FFprobe::probe(clip);
            if (!clipProbe.valid) {
                const QString errorText = QStringLiteral("Could not read %1: %2").arg(clip, clipProbe.errorMessage);
                job->setErrorMessage(errorText);
                job->setStatus(JobStatus::Failed);
                emit jobFinished(jobId, false, errorText);
                return;
            }
            totalDurationSeconds += clipProbe.durationSeconds;
        }
        args = buildVideoEditArgs(job);
    } else if (job->parameters().value(QStringLiteral("operation")).toString() == QStringLiteral("imageEdit")) {
        // Single frame in, single frame out — no meaningful progress% to
        // track (ffmpeg won't emit useful out_time for a still), but still
        // validate the input exists and is readable before spawning.
        const MediaProbeResult probeResult = FFprobe::probe(job->inputPath());
        if (!probeResult.valid) {
            job->setErrorMessage(probeResult.errorMessage);
            job->setStatus(JobStatus::Failed);
            emit jobFinished(jobId, false, probeResult.errorMessage);
            return;
        }
        args = buildImageEditArgs(job);
    } else if (job->parameters().value(QStringLiteral("operation")).toString() == QStringLiteral("audioEdit")) {
        // Same reasoning as the videoEdit branch above: multiple inputs, so
        // progress% is driven off the sum of each clip's real duration.
        QStringList clips{job->inputPath()};
        clips << job->extraInputPaths();
        for (const QString &clip : clips) {
            const MediaProbeResult clipProbe = FFprobe::probe(clip);
            if (!clipProbe.valid) {
                const QString errorText = QStringLiteral("Could not read %1: %2").arg(clip, clipProbe.errorMessage);
                job->setErrorMessage(errorText);
                job->setStatus(JobStatus::Failed);
                emit jobFinished(jobId, false, errorText);
                return;
            }
            totalDurationSeconds += clipProbe.durationSeconds;
        }
        args = buildAudioEditArgs(job);
    } else {
        const MediaProbeResult probeResult = FFprobe::probe(job->inputPath());
        if (!probeResult.valid) {
            job->setErrorMessage(probeResult.errorMessage);
            job->setStatus(JobStatus::Failed);
            emit jobFinished(jobId, false, probeResult.errorMessage);
            return;
        }
        totalDurationSeconds = probeResult.durationSeconds;
        args = buildArgsForJob(job, probeResult);
    }

    auto *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::SeparateChannels);

    RunningJob running;
    running.process = process;
    running.job = job;
    running.totalDurationSeconds = totalDurationSeconds;
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
    magnify::core::HostProcess::start(process, QStringLiteral("ffmpeg"), args);
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
