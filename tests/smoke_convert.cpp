// Standalone smoke test: drives the real JobManager + FFmpegMediaEngine
// (no mocks) through the three MVP conversions and exits non-zero on any
// failure. Not part of ctest (it shells out to a real ffmpeg process and
// needs synthetic input files) — run manually during MVP verification.
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
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

bool runOneConversion(JobManager &manager, const QString &input, const QString &output, const QString &targetFormat) {
    auto job = std::make_unique<ConversionJob>(input, output);
    job->setSourceFormat(QFileInfo(input).suffix().toLower());
    job->setTargetFormat(targetFormat);
    ConversionJob *raw = manager.addJob(std::move(job));

    QEventLoop loop;
    bool finished = false;
    bool ok = false;
    QObject::connect(raw, &ConversionJob::statusChanged, &loop, [&](JobStatus status) {
        if (status == JobStatus::Completed || status == JobStatus::Failed || status == JobStatus::Cancelled) {
            finished = true;
            ok = (status == JobStatus::Completed);
            loop.quit();
        }
    });
    QTimer::singleShot(60000, &loop, &QEventLoop::quit); // safety timeout

    manager.startQueue();
    loop.exec();

    if (!finished) {
        qWarning() << "TIMEOUT converting" << input << "->" << output;
        return false;
    }
    if (!ok) {
        qWarning() << "FAILED converting" << input << "->" << output << ":" << raw->errorMessage();
        return false;
    }
    if (!QFileInfo::exists(output) || QFileInfo(output).size() == 0) {
        qWarning() << "Output missing or empty:" << output;
        return false;
    }
    qInfo() << "OK:" << input << "->" << output << "(" << QFileInfo(output).size() << "bytes )";
    return true;
}

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    const QString dir = QDir::currentPath();
    const QString sourceMp4 = dir + "/sample.mp4";
    if (!QFileInfo::exists(sourceMp4)) {
        qWarning() << "sample.mp4 not found in" << dir << "- generate it with ffmpeg first.";
        return 1;
    }

    magnify::engines::ffmpeg::FFmpegMediaEngine engine;
    JobManager manager;
    manager.registerEngine(&engine);
    manager.setMaxConcurrentJobs(1);

    bool allOk = true;
    allOk &= runOneConversion(manager, sourceMp4, dir + "/out_from_mp4.mp3", "mp3");
    allOk &= runOneConversion(manager, sourceMp4, dir + "/out_from_mp4.mpeg", "mpeg");

    // MKV -> MP4: first remux the sample into an MKV container so we exercise
    // the real MKV input path, then convert MKV -> MP4 (should stream-copy).
    QProcess remux;
    remux.start("ffmpeg", {"-y", "-i", sourceMp4, "-c", "copy", dir + "/sample.mkv"});
    remux.waitForFinished(30000);
    allOk &= runOneConversion(manager, dir + "/sample.mkv", dir + "/out_from_mkv.mp4", "mp4");

    qInfo() << (allOk ? "ALL CONVERSIONS SUCCEEDED" : "SOME CONVERSIONS FAILED");
    return allOk ? 0 : 1;
}
