#include "ArchiveEngine.h"

#include <QDir>
#include <QFileInfo>

#include "core/ConversionJob.h"

using magnify::core::ConversionJob;
using magnify::core::JobStatus;

namespace magnify::engines::archive {

ArchiveEngine::ArchiveEngine(QObject *parent) : IMediaEngine(parent) {
}

MediaProbeResult ArchiveEngine::probe(const QString &filePath) {
    MediaProbeResult result;
    result.valid = QFileInfo(filePath).exists();
    if (!result.valid) {
        result.errorMessage = QStringLiteral("File does not exist: %1").arg(filePath);
    }
    return result;
}

void ArchiveEngine::startConversion(ConversionJob *job) {
    job->setStatus(JobStatus::Preparing);
    job->setStatus(JobStatus::Running);

    const QString operation = job->parameters().value(QStringLiteral("operation")).toString();
    if (operation == QStringLiteral("extract")) {
        extractArchive(job);
    } else if (operation == QStringLiteral("create")) {
        createArchive(job);
    } else {
        finishJob(job, false,
                   QStringLiteral("ArchiveEngine requires an 'operation' parameter (extract or create)."));
    }
}

void ArchiveEngine::extractArchive(ConversionJob *job) {
    QDir().mkpath(job->outputPath());
    // "x" preserves the archive's internal directory structure (vs "e",
    // which flattens everything into one folder).
    const QStringList args{QStringLiteral("x"), QStringLiteral("-y"),
                            QStringLiteral("-o") + job->outputPath(), job->inputPath()};

    runProcess(job, QStringLiteral("7z"), args,
               [this, job](QProcess *process, int exitCode, QProcess::ExitStatus exitStatus) {
                   const bool success = exitStatus == QProcess::NormalExit && exitCode == 0;
                   const QString error = success ? QString() : QString::fromUtf8(process->readAllStandardOutput());
                   finishJob(job, success, error);
               });
}

void ArchiveEngine::createArchive(ConversionJob *job) {
    QStringList args{QStringLiteral("a"), QStringLiteral("-y"), job->outputPath(), job->inputPath()};
    args << job->extraInputPaths();

    runProcess(job, QStringLiteral("7z"), args,
               [this, job](QProcess *process, int exitCode, QProcess::ExitStatus exitStatus) {
                   const bool success =
                       exitStatus == QProcess::NormalExit && exitCode == 0 && QFileInfo::exists(job->outputPath());
                   const QString error = success ? QString() : QString::fromUtf8(process->readAllStandardOutput());
                   finishJob(job, success, error);
               });
}

void ArchiveEngine::runProcess(ConversionJob *job, const QString &program, const QStringList &args,
                                ProcessFinishedHandler onFinished) {
    auto *process = new QProcess(this);
    m_runningProcesses.insert(job->id(), process);

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, job, process, onFinished = std::move(onFinished)](int exitCode, QProcess::ExitStatus exitStatus) {
                m_runningProcesses.remove(job->id());
                onFinished(process, exitCode, exitStatus);
                process->deleteLater();
            });
    // Without this, a process that fails to even start (7z missing from
    // PATH, permissions, ...) never emits finished(), and the job hangs
    // forever instead of failing with a clear message.
    connect(process, &QProcess::errorOccurred, this, [this, job, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || !m_runningProcesses.contains(job->id())) {
            return;
        }
        m_runningProcesses.remove(job->id());
        finishJob(job, false, QStringLiteral("Could not start %1: %2").arg(process->program(), process->errorString()));
        process->deleteLater();
    });

    process->start(program, args);
}

void ArchiveEngine::finishJob(ConversionJob *job, bool success, const QString &errorMessage) {
    if (success) {
        job->setProgressPercent(100);
        job->setStatus(JobStatus::Completed);
    } else {
        job->setErrorMessage(errorMessage);
        job->setStatus(JobStatus::Failed);
    }
    emit jobFinished(job->id(), success, errorMessage);
}

void ArchiveEngine::cancelConversion(const QUuid &jobId) {
    if (auto *process = m_runningProcesses.value(jobId)) {
        process->kill();
    }
}

} // namespace magnify::engines::archive
