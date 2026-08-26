// Standalone smoke test: applies real presets through JobManager +
// FFmpegMediaEngine and checks the produced files with ffprobe to confirm
// the preset's parameters (resolution, bitrate) actually took effect, not
// just that PresetRegistry returns the right struct. Not part of ctest —
// shells out to real ffmpeg/ffprobe processes.
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QProcess>
#include <QTimer>

#include "core/ConversionJob.h"
#include "core/JobManager.h"
#include "engines/ffmpeg/FFmpegMediaEngine.h"
#include "presets/PresetRegistry.h"

using magnify::core::ConversionJob;
using magnify::core::JobManager;
using magnify::core::JobStatus;
using magnify::presets::Preset;
using magnify::presets::PresetRegistry;

namespace {

bool runJob(JobManager &manager, const QString &input, const QString &output, const QString &sourceExt,
            const Preset &preset) {
    auto job = std::make_unique<ConversionJob>(input, output);
    job->setSourceFormat(sourceExt);
    job->setTargetFormat(preset.targetFormat);
    job->setEngineName("FFmpeg");
    job->setParameters(preset.parameters);
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
    QTimer::singleShot(30000, &loop, &QEventLoop::quit);
    manager.startQueue();
    loop.exec();

    if (!finished || !ok) {
        fprintf(stderr, "FAILED preset '%s': %s\n", qPrintable(preset.name), qPrintable(raw->errorMessage()));
        return false;
    }
    return true;
}

QString ffprobeValue(const QString &path, const QString &entry) {
    QProcess probe;
    probe.start("ffprobe", {"-v", "error", "-select_streams", "v:0", "-show_entries", entry, "-of",
                             "default=noprint_wrappers=1:nokey=1", path});
    probe.waitForFinished(10000);
    return QString::fromUtf8(probe.readAllStandardOutput()).trimmed();
}

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    const QString dir = QDir::currentPath();

    const QString sourcePath = dir + "/preset_src.mp4";
    QProcess gen;
    gen.start("ffmpeg", {"-y", "-f", "lavfi", "-i", "testsrc=duration=2:size=640x360:rate=30", "-f", "lavfi", "-i",
                          "sine=frequency=1000:duration=2", "-c:v", "libx264", "-c:a", "aac", sourcePath});
    gen.waitForFinished(15000);
    if (!QFileInfo::exists(sourcePath)) {
        fprintf(stderr, "Could not generate synthetic source\n");
        return 1;
    }

    magnify::engines::ffmpeg::FFmpegMediaEngine engine;
    JobManager manager;
    manager.registerEngine(&engine);

    bool allOk = true;

    // "Discord" preset: mp4, crf 28, 1280x720, 128k audio.
    const Preset discordPreset = [] {
        for (const Preset &p : PresetRegistry::instance().all()) {
            if (p.name == QStringLiteral("Discord")) return p;
        }
        return Preset{};
    }();
    if (discordPreset.name.isEmpty()) {
        fprintf(stderr, "Discord preset not found in registry\n");
        return 1;
    }
    const QString discordOut = dir + "/preset_discord.mp4";
    if (runJob(manager, sourcePath, discordOut, "mp4", discordPreset)) {
        const QString width = ffprobeValue(discordOut, "stream=width");
        const QString height = ffprobeValue(discordOut, "stream=height");
        if (width == "1280" && height == "720") {
            fprintf(stderr, "OK: Discord preset resized to %sx%s as requested\n", qPrintable(width), qPrintable(height));
        } else {
            fprintf(stderr, "FAILED: Discord preset produced %sx%s, expected 1280x720\n", qPrintable(width),
                     qPrintable(height));
            allOk = false;
        }
    } else {
        allOk = false;
    }

    // "MP3 320 kbps" preset: confirm ffprobe reports a bitrate close to 320k.
    const Preset mp3Preset = [] {
        for (const Preset &p : PresetRegistry::instance().all()) {
            if (p.name == QStringLiteral("MP3 320 kbps")) return p;
        }
        return Preset{};
    }();
    const QString mp3Out = dir + "/preset_mp3.mp3";
    if (runJob(manager, sourcePath, mp3Out, "mp4", mp3Preset)) {
        QProcess probe;
        probe.start("ffprobe", {"-v", "error", "-show_entries", "format=bit_rate", "-of",
                                 "default=noprint_wrappers=1:nokey=1", mp3Out});
        probe.waitForFinished(10000);
        const int bitrate = QString::fromUtf8(probe.readAllStandardOutput()).trimmed().toInt();
        // MP3 CBR framing/ID3 overhead means the measured rate is close to but
        // not exactly 320000; allow a reasonable margin either side.
        if (bitrate > 300000 && bitrate <= 345000) {
            fprintf(stderr, "OK: MP3 320kbps preset measured at %d bps\n", bitrate);
        } else {
            fprintf(stderr, "FAILED: MP3 320kbps preset measured at %d bps, expected ~320000\n", bitrate);
            allOk = false;
        }
    } else {
        allOk = false;
    }

    fprintf(stderr, "%s\n", allOk ? "ALL PRESET CHECKS PASSED" : "SOME PRESET CHECKS FAILED");
    return allOk ? 0 : 1;
}
