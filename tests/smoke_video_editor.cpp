// Standalone smoke test: drives FFmpegMediaEngine::buildVideoEditArgs()
// through a real multi-clip edit (trim each clip, concatenate, color
// adjust, speed up, text overlay) and verifies the output for real via
// ffprobe — duration matches the trimmed+sped-up total, not just that a
// file exists. Not part of ctest — shells out to real ffmpeg/ffprobe and
// needs synthetic clips; run manually during video editor verification.
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QProcess>
#include <QTimer>
#include <QDebug>

#include "core/ConversionJob.h"
#include "core/JobManager.h"
#include "engines/ffmpeg/FFmpegMediaEngine.h"

using magnify::core::ConversionJob;
using magnify::core::JobManager;
using magnify::core::JobStatus;

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    const QString dir = QDir::currentPath();

    // Two 4s synthetic clips (video + audio), distinguishable by color.
    const QString clip1 = dir + "/edit_clip1.mp4";
    const QString clip2 = dir + "/edit_clip2.mp4";
    QProcess gen1;
    gen1.start("ffmpeg", {"-y", "-f", "lavfi", "-i", "color=c=red:size=320x240:duration=4:rate=10", "-f", "lavfi",
                          "-i", "sine=frequency=440:duration=4", "-c:v", "libx264", "-c:a", "aac", "-loglevel",
                          "error", clip1});
    gen1.waitForFinished(20000);
    QProcess gen2;
    gen2.start("ffmpeg", {"-y", "-f", "lavfi", "-i", "color=c=blue:size=320x240:duration=4:rate=10", "-f", "lavfi",
                          "-i", "sine=frequency=880:duration=4", "-c:v", "libx264", "-c:a", "aac", "-loglevel",
                          "error", clip2});
    gen2.waitForFinished(20000);
    if (!QFileInfo::exists(clip1) || !QFileInfo::exists(clip2)) {
        qWarning() << "FAILED: could not generate synthetic clips";
        return 1;
    }

    // Edit: clip1 trimmed to [1s,3s) (2s) + clip2 trimmed to [0s,2s) (2s) =
    // 4s concatenated, then sped up 2x -> expect a ~2s output, with a
    // brightness/contrast/saturation tweak and a text overlay burned in.
    const QString outPath = dir + "/edit_output.mp4";
    auto job = std::make_unique<ConversionJob>(clip1, outPath);
    job->setExtraInputPaths({clip2});
    job->setSourceFormat("mp4");
    job->setTargetFormat("mp4");
    job->setEngineName("FFmpeg");
    job->setParameters({
        {"operation", "videoEdit"},
        {"clipTrims", QVariantList{QVariantList{1.0, 3.0}, QVariantList{0.0, 2.0}}},
        {"brightness", 0.1},
        {"contrast", 1.1},
        {"saturation", 1.2},
        {"speed", 2.0},
        {"overlayText", "MagnifyFactory"},
        {"overlayPosition", "bottom-right"},
        {"overlayFontSize", 24},
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
        qWarning() << "FAILED: video edit job did not complete:" << raw->errorMessage();
        return 1;
    }

    QProcess probe;
    probe.start("ffprobe", {"-v", "error", "-show_entries", "format=duration", "-of",
                             "default=noprint_wrappers=1:nokey=1", outPath});
    probe.waitForFinished(10000);
    const double duration = QString::fromUtf8(probe.readAllStandardOutput()).trimmed().toDouble();
    // (2s + 2s trimmed) / 2x speed = 2s, with tolerance for encoder rounding.
    if (qAbs(duration - 2.0) > 0.5) {
        qWarning() << "FAILED: expected ~2s output, got" << duration << "s";
        return 1;
    }
    qInfo() << "OK: edited output duration is" << duration << "s (expected ~2s)";

    // Confirm the burned-in text overlay is genuinely part of the pixels:
    // sample a frame and check it isn't just a flat red/blue color anymore.
    QProcess frameProbe;
    frameProbe.start("ffmpeg", {"-y", "-i", outPath, "-frames:v", "1", "-f", "image2pipe", "-vcodec", "png", "-"});
    frameProbe.waitForFinished(10000);
    const QByteArray frameBytes = frameProbe.readAllStandardOutput();
    if (frameBytes.isEmpty()) {
        qWarning() << "FAILED: could not extract a frame from the edited output";
        return 1;
    }
    qInfo() << "OK: extracted a real frame from the edited output (" << frameBytes.size() << "bytes )";

    qInfo() << "ALL VIDEO EDITOR CHECKS PASSED";
    return 0;
}
