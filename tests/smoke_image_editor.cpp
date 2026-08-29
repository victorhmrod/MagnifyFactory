// Standalone smoke test: drives FFmpegMediaEngine::buildImageEditArgs()
// through a real crop + resize + color/blur + text overlay edit and
// verifies the output for real via ffprobe (exact output dimensions) and a
// pixel check (the crop actually cut out the expected region). Not part of
// ctest — shells out to real ffmpeg/ffprobe; run manually during image
// editor verification.
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QImage>
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

    // A 400x300 source: left half red, right half blue — lets the crop
    // check confirm exactly which region survived.
    QImage source(400, 300, QImage::Format_RGB32);
    source.fill(QColor(255, 0, 0));
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 200; x < source.width(); ++x) {
            source.setPixel(x, y, qRgb(0, 0, 255));
        }
    }
    const QString srcPath = dir + "/imgedit_src.png";
    if (!source.save(srcPath, "PNG")) {
        qWarning() << "FAILED: could not write synthetic source image";
        return 1;
    }

    // Crop to the left (red) half: x=0,y=0,w=200,h=300. Resize to 100x150.
    // Brightness/contrast/saturation tweak, a touch of blur, text overlay.
    const QString outPath = dir + "/imgedit_out.png";
    auto job = std::make_unique<ConversionJob>(srcPath, outPath);
    job->setSourceFormat("png");
    job->setTargetFormat("png");
    job->setEngineName("FFmpeg");
    job->setParameters({
        {"operation", "imageEdit"},
        {"crop", QVariantList{0, 0, 200, 300}},
        {"resizeWidth", 100},
        {"resizeHeight", 150},
        {"brightness", 0.05},
        {"contrast", 1.05},
        {"saturation", 1.1},
        {"blur", 1.5},
        {"overlayText", "Edited"},
        {"overlayPosition", "top-left"},
        {"overlayFontSize", 14},
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
    QTimer::singleShot(30000, &loop, &QEventLoop::quit);
    manager.startQueue();
    loop.exec();

    if (!finished || !ok) {
        qWarning() << "FAILED: image edit job did not complete:" << raw->errorMessage();
        return 1;
    }

    QProcess probe;
    probe.start("ffprobe", {"-v", "error", "-select_streams", "v:0", "-show_entries", "stream=width,height", "-of",
                             "csv=p=0", outPath});
    probe.waitForFinished(10000);
    const QString dims = QString::fromUtf8(probe.readAllStandardOutput()).trimmed();
    if (dims != "100,150") {
        qWarning() << "FAILED: expected 100x150 output, got" << dims;
        return 1;
    }
    qInfo() << "OK: cropped+resized output is" << dims;

    // Pixel check: the crop kept only the red half, so the output's average
    // color should be red-dominant, not blue — confirms the crop region
    // (not just its dimensions) is correct.
    QImage result(outPath);
    if (result.isNull()) {
        qWarning() << "FAILED: could not load the edited output as an image";
        return 1;
    }
    qint64 redSum = 0, blueSum = 0;
    for (int y = 0; y < result.height(); ++y) {
        for (int x = 0; x < result.width(); ++x) {
            const QRgb px = result.pixel(x, y);
            redSum += qRed(px);
            blueSum += qBlue(px);
        }
    }
    if (redSum <= blueSum) {
        qWarning() << "FAILED: expected the red half to survive the crop (red sum" << redSum << "<= blue sum"
                    << blueSum << ")";
        return 1;
    }
    qInfo() << "OK: crop kept the correct region (red sum" << redSum << "> blue sum" << blueSum << ")";

    qInfo() << "ALL IMAGE EDITOR CHECKS PASSED";
    return 0;
}
