// Standalone smoke test: drives the real JobManager + FFmpegMediaEngine
// through a trim job (trimStart/trimEnd parameters) and verifies, via a real
// ffprobe call on the output, that the resulting duration actually matches
// the requested cut — not just that a file was produced. Not part of ctest
// (shells out to real ffmpeg/ffprobe and needs a synthetic input); run
// manually during Trim tool verification.
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
#include "engines/ffmpeg/FFprobe.h"

using magnify::core::ConversionJob;
using magnify::core::JobManager;
using magnify::core::JobStatus;
using magnify::engines::ffmpeg::FFprobe;

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    const QString dir = QDir::currentPath();
    const QString sourcePath = dir + "/trim_smoke_src.mp4";
    const QString outputPath = dir + "/trim_smoke_out.mp4";

    // A 10-second synthetic clip with both video and audio.
    QProcess gen;
    gen.start("ffmpeg", {"-y", "-f", "lavfi", "-i", "testsrc=duration=10:size=320x240:rate=10", "-f", "lavfi", "-i",
                          "sine=frequency=440:duration=10", "-shortest", "-c:v", "libx264", "-c:a", "aac",
                          sourcePath});
    gen.waitForFinished(30000);
    if (!QFileInfo::exists(sourcePath)) {
        qWarning() << "FAILED: could not generate synthetic source video";
        return 1;
    }

    magnify::engines::ffmpeg::FFmpegMediaEngine engine;
    JobManager manager;
    manager.registerEngine(&engine);

    // Trim to the [2s, 5s) slice -> expect ~3s of output.
    auto job = std::make_unique<ConversionJob>(sourcePath, outputPath);
    job->setSourceFormat("mp4");
    job->setTargetFormat("mp4");
    job->setEngineName("FFmpeg");
    job->setParameters({{"trimStart", 2.0}, {"trimEnd", 5.0}});
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
        qWarning() << "FAILED: trim job did not complete:" << raw->errorMessage();
        return 1;
    }

    const auto probe = FFprobe::probe(outputPath);
    if (!probe.valid) {
        qWarning() << "FAILED: could not probe trimmed output";
        return 1;
    }

    // Allow a small tolerance for encoder rounding at GOP boundaries.
    const double expectedDuration = 3.0;
    if (qAbs(probe.durationSeconds - expectedDuration) > 0.5) {
        qWarning() << "FAILED: expected ~" << expectedDuration << "s, got" << probe.durationSeconds << "s";
        return 1;
    }

    qInfo() << "OK: trimmed output duration is" << probe.durationSeconds << "s (expected ~" << expectedDuration
            << "s)";
    qInfo() << "ALL TRIM CHECKS PASSED";
    return 0;
}
