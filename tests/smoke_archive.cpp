// Standalone smoke test: drives the real JobManager + ArchiveEngine through
// create (multi-file -> zip) and extract (zip -> folder), then verifies the
// extracted files are byte-identical to the originals. Not part of ctest —
// shells out to the real 7z.exe; run manually during verification.
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

#include "core/ConversionJob.h"
#include "core/JobManager.h"
#include "engines/archive/ArchiveEngine.h"

using magnify::core::ConversionJob;
using magnify::core::JobManager;
using magnify::core::JobStatus;

namespace {

bool runJob(JobManager &manager, std::unique_ptr<ConversionJob> jobPtr) {
    ConversionJob *raw = manager.addJob(std::move(jobPtr));

    QEventLoop loop;
    bool finished = false, ok = false;
    QObject::connect(raw, &ConversionJob::statusChanged, &loop, [&](JobStatus status) {
        if (status == JobStatus::Completed || status == JobStatus::Failed) {
            finished = true;
            ok = (status == JobStatus::Completed);
            loop.quit();
        }
    });
    QTimer::singleShot(30000, &loop, &QEventLoop::quit);
    manager.startQueue();
    loop.exec();

    if (!finished || !ok) {
        fprintf(stderr, "FAILED: %s\n", qPrintable(raw->errorMessage()));
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    const QString dir = QDir::currentPath();

    // Two distinct source files with known content.
    const QString fileA = dir + "/a.txt";
    const QString fileB = dir + "/b.txt";
    QFile(fileA).open(QIODevice::WriteOnly | QIODevice::Truncate);
    {
        QFile f(fileA);
        f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        f.write("Hello from file A\n");
    }
    {
        QFile f(fileB);
        f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        f.write("Hello from file B, slightly longer content.\n");
    }

    magnify::engines::archive::ArchiveEngine engine;
    JobManager manager;
    manager.registerEngine(&engine);

    bool allOk = true;

    // Create: a.txt + b.txt -> bundle.zip
    const QString zipPath = dir + "/bundle.zip";
    {
        auto job = std::make_unique<ConversionJob>(fileA, zipPath);
        job->setExtraInputPaths({fileB});
        job->setSourceFormat("txt");
        job->setTargetFormat("zip");
        job->setEngineName("Archive Tools");
        job->setParameters({{"operation", "create"}});
        allOk &= runJob(manager, std::move(job));
    }
    if (!QFileInfo::exists(zipPath)) {
        fprintf(stderr, "FAILED: bundle.zip was not created\n");
        return 1;
    }
    fprintf(stderr, "OK: created %s (%lld bytes)\n", qPrintable(zipPath), (long long)QFileInfo(zipPath).size());

    // Extract: bundle.zip -> extracted/
    const QString extractDir = dir + "/extracted";
    {
        auto job = std::make_unique<ConversionJob>(zipPath, extractDir);
        job->setSourceFormat("zip");
        job->setTargetFormat("folder");
        job->setEngineName("Archive Tools");
        job->setParameters({{"operation", "extract"}});
        allOk &= runJob(manager, std::move(job));
    }

    const QString extractedA = extractDir + "/a.txt";
    const QString extractedB = extractDir + "/b.txt";
    auto readAll = [](const QString &path) {
        QFile f(path);
        f.open(QIODevice::ReadOnly);
        return f.readAll();
    };
    const bool contentMatches =
        QFileInfo::exists(extractedA) && QFileInfo::exists(extractedB) &&
        readAll(extractedA) == readAll(fileA) && readAll(extractedB) == readAll(fileB);

    if (!contentMatches) {
        fprintf(stderr, "FAILED: extracted files missing or content mismatch\n");
        allOk = false;
    } else {
        fprintf(stderr, "OK: extracted files are byte-identical to the originals\n");
    }

    fprintf(stderr, "%s\n", allOk ? "ALL ARCHIVE CHECKS PASSED" : "SOME ARCHIVE CHECKS FAILED");
    return allOk ? 0 : 1;
}
