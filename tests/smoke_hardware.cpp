// Standalone smoke test: exercises real GPU-encoder detection (shells out to
// ffmpeg for real test encodes) and confirms FFmpegMediaEngine actually
// drives a hardware encoder end to end when one is available. Not part of
// ctest — hardware-dependent; run manually during verification.
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QProcess>
#include <QTimer>

#include "core/ConversionJob.h"
#include "core/JobManager.h"
#include "engines/ffmpeg/FFmpegMediaEngine.h"
#include "hardware/HardwareAccelerationManager.h"

using magnify::core::ConversionJob;
using magnify::core::JobManager;
using magnify::core::JobStatus;
using magnify::hardware::HardwareAccelerationManager;
using magnify::hardware::HardwareVendor;
using magnify::hardware::hardwareVendorToString;

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    const QString dir = QDir::currentPath();

    auto &hw = HardwareAccelerationManager::instance();
    const QList<HardwareVendor> vendors = hw.availableVendors("h264");
    fprintf(stderr, "Detected usable vendors: ");
    for (auto v : vendors) fprintf(stderr, "%s ", qPrintable(hardwareVendorToString(v)));
    fprintf(stderr, "\n");

    if (vendors.size() <= 1) {
        fprintf(stderr, "No GPU encoder verified as usable on this machine (CPU only) — nothing more to test.\n");
        return 0;
    }

    const HardwareVendor gpuVendor = vendors[1]; // first non-CPU entry
    const QString encoder = hw.encoderFor(gpuVendor, "h264");
    fprintf(stderr, "Will use encoder '%s' for vendor '%s'\n", qPrintable(encoder),
            qPrintable(hardwareVendorToString(gpuVendor)));

    // Generate a short synthetic source, then run it through the real
    // FFmpegMediaEngine + JobManager with that hardware backend selected.
    const QString sourcePath = dir + "/hwtest_src.mp4";
    {
        QProcess gen;
        gen.start("ffmpeg", {"-y", "-f", "lavfi", "-i", "testsrc=duration=2:size=640x360:rate=30", "-c:v", "libx264",
                              sourcePath});
        gen.waitForFinished(15000);
    }
    if (!QFileInfo::exists(sourcePath)) {
        fprintf(stderr, "Could not generate synthetic source video\n");
        return 1;
    }

    magnify::engines::ffmpeg::FFmpegMediaEngine engine;
    JobManager manager;
    manager.registerEngine(&engine);

    auto job = std::make_unique<ConversionJob>(sourcePath, dir + "/hwtest_out.mkv");
    job->setSourceFormat("mp4");
    job->setTargetFormat("mkv");
    job->setEngineName("FFmpeg");
    job->setParameters({{"hardwareBackend", hardwareVendorToString(gpuVendor)}});
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
        fprintf(stderr, "FAILED hardware-accelerated encode: %s\n", qPrintable(raw->errorMessage()));
        return 1;
    }
    fprintf(stderr, "OK: hardware-accelerated encode succeeded (%lld bytes)\n",
            (long long)QFileInfo(raw->outputPath()).size());
    return 0;
}
