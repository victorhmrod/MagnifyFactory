// Standalone smoke test: drives the real JobManager + FFmpegMediaEngine
// through image rotate (params["rotate"]) and subtitle extraction
// (params["operation"]="extractSubtitles"), verifying the results via real
// ffprobe calls rather than just checking a file exists. Not part of ctest
// (shells out to real ffmpeg/ffprobe and needs synthetic input); run
// manually during rotate/subtitle verification.
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
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

namespace {

bool runJob(JobManager &manager, const QString &input, const QString &output, const QString &sourceExt,
            const QString &targetExt, const QVariantMap &params, QString *errorOut = nullptr) {
    auto job = std::make_unique<ConversionJob>(input, output);
    job->setSourceFormat(sourceExt);
    job->setTargetFormat(targetExt);
    job->setEngineName("FFmpeg");
    job->setParameters(params);
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

    if (errorOut) {
        *errorOut = raw->errorMessage();
    }
    return finished && ok;
}

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    const QString dir = QDir::currentPath();
    bool allOk = true;

    // --- Rotate: a 320x240 source rotated 90 CW should come out 240x320 ---
    {
        const QString srcPath = dir + "/rotate_smoke_src.png";
        const QString outPath = dir + "/rotate_smoke_out.png";
        QProcess gen;
        gen.start("ffmpeg", {"-y", "-f", "lavfi", "-i", "testsrc=size=320x240", "-frames:v", "1", srcPath});
        gen.waitForFinished(15000);

        magnify::engines::ffmpeg::FFmpegMediaEngine engine;
        JobManager manager;
        manager.registerEngine(&engine);
        QString error;
        const bool jobOk = runJob(manager, srcPath, outPath, "png", "png", {{"rotate", 90}}, &error);

        QProcess probe;
        probe.start("ffprobe", {"-v", "error", "-select_streams", "v:0", "-show_entries", "stream=width,height",
                                 "-of", "csv=p=0", outPath});
        probe.waitForFinished(10000);
        const QString dims = QString::fromUtf8(probe.readAllStandardOutput()).trimmed();

        if (!jobOk || dims != "240,320") {
            qWarning() << "FAILED: rotate job. ok=" << jobOk << "dims=" << dims << "error=" << error;
            allOk = false;
        } else {
            qInfo() << "OK: 320x240 rotated 90 degrees ->" << dims;
        }
    }

    // --- Extract subtitles: a synthetic mkv with a burned-in soft subtitle
    // track should produce a non-empty .srt containing the sample text.
    {
        const QString srcPath = dir + "/subs_smoke_src.srt";
        const QString videoPath = dir + "/subs_smoke_src.mkv";
        const QString outPath = dir + "/subs_smoke_out.srt";

        QFile srtFile(srcPath);
        srtFile.open(QIODevice::WriteOnly | QIODevice::Text);
        srtFile.write("1\n00:00:00,000 --> 00:00:02,000\nMagnifyFactory subtitle smoke test\n\n");
        srtFile.close();

        QProcess mux;
        mux.start("ffmpeg", {"-y", "-f", "lavfi", "-i", "testsrc=duration=2:size=320x240:rate=10", "-i", srcPath,
                              "-c:v", "libx264", "-c:s", "srt", videoPath});
        mux.waitForFinished(20000);

        magnify::engines::ffmpeg::FFmpegMediaEngine engine;
        JobManager manager;
        manager.registerEngine(&engine);
        QString error;
        const bool jobOk =
            runJob(manager, videoPath, outPath, "mkv", "srt", {{"operation", "extractSubtitles"}}, &error);

        QFile result(outPath);
        const bool opened = result.open(QIODevice::ReadOnly | QIODevice::Text);
        const QString content = opened ? QString::fromUtf8(result.readAll()) : QString();

        if (!jobOk || !content.contains("MagnifyFactory subtitle smoke test")) {
            qWarning() << "FAILED: subtitle extraction. ok=" << jobOk << "error=" << error << "content=" << content;
            allOk = false;
        } else {
            qInfo() << "OK: extracted subtitle contains the expected text";
        }
    }

    qInfo() << (allOk ? "ALL MEDIA EDIT CHECKS PASSED" : "SOME MEDIA EDIT CHECKS FAILED");
    return allOk ? 0 : 1;
}
