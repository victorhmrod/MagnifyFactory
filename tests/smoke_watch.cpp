// Standalone smoke test: real WatchFolderManager watching a real directory,
// wired to the real JobManager + FFmpegMediaEngine. Copies a video into the
// watched folder mid-run and confirms the conversion fires automatically,
// without any UI. Not part of ctest — takes several real seconds (settle
// delay + actual encode); run manually during verification.
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTimer>

#include "core/ConversionJob.h"
#include "core/JobManager.h"
#include "engines/ffmpeg/FFmpegMediaEngine.h"
#include "watch/WatchFolderManager.h"

using magnify::core::ConversionJob;
using magnify::core::JobManager;
using magnify::core::JobStatus;
using magnify::watch::WatchFolderManager;
using magnify::watch::WatchRule;

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    const QString dir = QDir::currentPath();
    const QString watchedDir = dir + "/watched";
    QDir().mkpath(watchedDir);

    const QString sourcePath = dir + "/watch_src.mp4";
    QProcess gen;
    gen.start("ffmpeg", {"-y", "-f", "lavfi", "-i", "testsrc=duration=1:size=320x240:rate=10", "-f", "lavfi", "-i",
                          "sine=frequency=1000:duration=1", "-c:v", "libx264", "-c:a", "aac", sourcePath});
    gen.waitForFinished(15000);
    if (!QFileInfo::exists(sourcePath)) {
        fprintf(stderr, "Could not generate synthetic source video\n");
        return 1;
    }

    magnify::engines::ffmpeg::FFmpegMediaEngine engine;
    JobManager manager;
    manager.registerEngine(&engine);

    WatchFolderManager watcher;
    WatchRule rule;
    rule.folderPath = watchedDir;
    rule.targetExt = "mp3";
    rule.enabled = true;
    watcher.addRule(rule);

    bool detected = false;
    bool converted = false;
    QEventLoop loop;

    QObject::connect(&watcher, &WatchFolderManager::fileDetected, &loop,
                      [&](const QString &filePath, const WatchRule &r) {
                          detected = true;
                          auto job = std::make_unique<ConversionJob>(filePath, watchedDir + "/dropped.mp3");
                          job->setSourceFormat("mp4");
                          job->setTargetFormat(r.targetExt);
                          job->setEngineName("FFmpeg");
                          ConversionJob *raw = manager.addJob(std::move(job));
                          QObject::connect(raw, &ConversionJob::statusChanged, &loop, [&, raw](JobStatus status) {
                              if (status == JobStatus::Completed || status == JobStatus::Failed) {
                                  converted = (status == JobStatus::Completed);
                                  if (!converted) {
                                      fprintf(stderr, "Job error: %s\n", qPrintable(raw->errorMessage()));
                                  }
                                  loop.quit();
                              }
                          });
                          manager.startQueue();
                      });

    // Drop the file into the watched folder ~1s after the loop starts, so
    // the watcher (already armed) sees a real directoryChanged event.
    QTimer::singleShot(1000, &app, [&]() {
        QFile::copy(sourcePath, watchedDir + "/dropped.mp4");
        fprintf(stderr, "Copied file into watched folder\n");
    });

    QTimer::singleShot(20000, &loop, &QEventLoop::quit); // safety timeout: settle delay + encode
    loop.exec();

    if (!detected) {
        fprintf(stderr, "FAILED: watch folder never detected the new file\n");
        return 1;
    }
    if (!converted || !QFileInfo::exists(watchedDir + "/dropped.mp3")) {
        fprintf(stderr, "FAILED: detected the file but conversion did not complete\n");
        return 1;
    }
    fprintf(stderr, "OK: watch folder detected and auto-converted dropped.mp4 -> dropped.mp3 (%lld bytes)\n",
            (long long)QFileInfo(watchedDir + "/dropped.mp3").size());
    return 0;
}
