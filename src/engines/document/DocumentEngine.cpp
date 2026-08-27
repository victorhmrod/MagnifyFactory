#include "DocumentEngine.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include "core/ConversionJob.h"

using magnify::core::ConversionJob;
using magnify::core::JobStatus;

namespace magnify::engines::document {

DocumentEngine::DocumentEngine(QObject *parent) : IMediaEngine(parent) {
}

QString DocumentEngine::sofficeExecutable() {
    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("soffice"));
    if (!onPath.isEmpty()) {
        return onPath;
    }
    const QStringList candidates{
        QStringLiteral("C:/Program Files/LibreOffice/program/soffice.exe"),
        QStringLiteral("C:/Program Files (x86)/LibreOffice/program/soffice.exe"),
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return QString();
}

MediaProbeResult DocumentEngine::probe(const QString &filePath) {
    MediaProbeResult result;
    result.valid = QFileInfo(filePath).exists();
    if (!result.valid) {
        result.errorMessage = QStringLiteral("File does not exist: %1").arg(filePath);
    }
    return result;
}

void DocumentEngine::startConversion(ConversionJob *job) {
    job->setStatus(JobStatus::Preparing);

    const QString soffice = sofficeExecutable();
    if (soffice.isEmpty()) {
        finishJob(job, false,
                  QStringLiteral("LibreOffice (soffice) was not found. Install it to convert documents."));
        return;
    }

    const QFileInfo outInfo(job->outputPath());
    const QString outDir = outInfo.absolutePath();
    QDir().mkpath(outDir);

    const QFileInfo inInfo(job->inputPath());
    const QString targetExt = job->targetFormat().toLower();
    // soffice writes "<source-basename>.<targetExt>" into --outdir; it does
    // not accept an explicit output filename, so we predict that path and
    // rename it to job->outputPath() afterwards (same dance PdfEngine does
    // for pdftoppm).
    const QString producedPath = QDir(outDir).filePath(inInfo.completeBaseName() + QStringLiteral(".") + targetExt);

    // LibreOffice needs its own isolated user profile directory per
    // concurrent instance, or parallel jobs corrupt each other's lock files.
    // Give each job a private one under the temp dir.
    const QString profileDir = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                    .filePath(QStringLiteral("magnify_soffice_profile_%1").arg(job->id().toString(QUuid::WithoutBraces)));
    const QString userInstallArg =
        QStringLiteral("-env:UserInstallation=file:///%1").arg(QDir::fromNativeSeparators(profileDir));

    const QStringList args{
        QStringLiteral("--headless"),
        QStringLiteral("--norestore"),
        userInstallArg,
        QStringLiteral("--convert-to"), targetExt,
        QStringLiteral("--outdir"), outDir,
        job->inputPath(),
    };

    job->setStatus(JobStatus::Running);

    auto *process = new QProcess(this);
    m_runningProcesses.insert(job->id(), process);

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, job, process, producedPath, profileDir](int exitCode, QProcess::ExitStatus exitStatus) {
                m_runningProcesses.remove(job->id());

                bool success =
                    exitStatus == QProcess::NormalExit && exitCode == 0 && QFileInfo::exists(producedPath);
                if (success && producedPath != job->outputPath()) {
                    QFile::remove(job->outputPath());
                    success = QFile::rename(producedPath, job->outputPath());
                }

                const QString error =
                    success ? QString()
                            : (QStringLiteral("LibreOffice conversion failed: ") +
                               QString::fromUtf8(process->readAllStandardOutput() + process->readAllStandardError()));
                finishJob(job, success, error);

                QDir(profileDir).removeRecursively();
                process->deleteLater();
            });

    connect(process, &QProcess::errorOccurred, this, [this, job](QProcess::ProcessError) {
        if (m_runningProcesses.contains(job->id())) {
            finishJob(job, false, QStringLiteral("Failed to start LibreOffice (soffice)."));
        }
    });

    process->start(soffice, args);
}

void DocumentEngine::finishJob(ConversionJob *job, bool success, const QString &errorMessage) {
    if (success) {
        job->setProgressPercent(100);
        job->setStatus(JobStatus::Completed);
    } else {
        job->setErrorMessage(errorMessage);
        job->setStatus(JobStatus::Failed);
    }
    emit jobFinished(job->id(), success, errorMessage);
}

void DocumentEngine::cancelConversion(const QUuid &jobId) {
    if (auto *process = m_runningProcesses.value(jobId)) {
        process->kill();
    }
}

} // namespace magnify::engines::document
