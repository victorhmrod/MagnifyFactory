#include "PdfEngine.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QUuid>

#include "PdfImageWriter.h"
#include "core/ConversionJob.h"

using magnify::core::ConversionJob;
using magnify::core::JobStatus;

namespace magnify::engines::pdf {

PdfEngine::PdfEngine(QObject *parent) : IMediaEngine(parent) {
}

MediaProbeResult PdfEngine::probe(const QString &filePath) {
    // Full PDF metadata (page count, etc.) is future work; the UI does not
    // currently need it for PDF <-> image jobs, only a validity check.
    MediaProbeResult result;
    result.valid = QFileInfo(filePath).exists();
    if (!result.valid) {
        result.errorMessage = QStringLiteral("File does not exist: %1").arg(filePath);
    }
    return result;
}

void PdfEngine::startConversion(ConversionJob *job) {
    job->setStatus(JobStatus::Preparing);
    job->setStatus(JobStatus::Running);

    const QString sourceExt = job->sourceFormat().toLower();
    const QString targetExt = job->targetFormat().toLower();

    if (sourceExt == QStringLiteral("pdf") && targetExt == QStringLiteral("pdf")) {
        compressPdf(job);
    } else if (sourceExt == QStringLiteral("pdf")) {
        convertPdfToImage(job);
    } else if (targetExt == QStringLiteral("pdf")) {
        convertImageToPdf(job);
    } else {
        finishJob(job, false, QStringLiteral("PDF Tools cannot convert %1 to %2").arg(sourceExt, targetExt));
    }
}

void PdfEngine::convertPdfToImage(ConversionJob *job) {
    const QString targetExt = job->targetFormat().toLower();
    const bool isJpeg = targetExt == QStringLiteral("jpg") || targetExt == QStringLiteral("jpeg");

    // pdftoppm appends its own extension to the prefix; with -singlefile (and
    // a single-page range) it writes exactly "<prefix>.<ext>" with no page
    // number suffix, which lets us target ConversionJob's single output path.
    const QFileInfo outputInfo(job->outputPath());
    const QString outputPrefix = QDir(outputInfo.absolutePath()).filePath(outputInfo.completeBaseName());

    QStringList args{
        isJpeg ? QStringLiteral("-jpeg") : QStringLiteral("-png"),
        QStringLiteral("-singlefile"),
        QStringLiteral("-r"), QStringLiteral("150"),
        QStringLiteral("-f"), QStringLiteral("1"),
        QStringLiteral("-l"), QStringLiteral("1"),
        job->inputPath(),
        outputPrefix,
    };

    auto *process = new QProcess(this);
    m_runningProcesses.insert(job->id(), process);

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, job, process, outputPrefix, targetExt](int exitCode, QProcess::ExitStatus exitStatus) {
                const QString producedPath = outputPrefix + QStringLiteral(".") + targetExt;
                const bool success = exitStatus == QProcess::NormalExit && exitCode == 0 &&
                                      QFileInfo::exists(producedPath);
                if (success && producedPath != job->outputPath()) {
                    QFile::remove(job->outputPath());
                    QFile::rename(producedPath, job->outputPath());
                }
                const QString error = success ? QString() : QString::fromUtf8(process->readAllStandardError());
                m_runningProcesses.remove(job->id());
                process->deleteLater();
                finishJob(job, success, error);
            });

    process->start(QStringLiteral("pdftoppm"), args);
}

void PdfEngine::convertImageToPdf(ConversionJob *job) {
    const QString sourceExt = job->sourceFormat().toLower();

    if (sourceExt == QStringLiteral("jpg") || sourceExt == QStringLiteral("jpeg")) {
        QString error;
        const bool ok = PdfImageWriter::writeSingleImagePdf(job->inputPath(), job->outputPath(), &error);
        finishJob(job, ok, error);
        return;
    }

    // Non-JPEG source (PNG/WebP/BMP/...): re-encode to a temp JPEG with
    // FFmpeg first, since PdfImageWriter only embeds JPEG (DCTDecode).
    auto *tempFile = new QTemporaryFile(
        QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath("magnify_XXXXXX.jpg"), this);
    tempFile->setAutoRemove(false);
    if (!tempFile->open()) {
        finishJob(job, false, QStringLiteral("Could not create a temporary file for image conversion."));
        tempFile->deleteLater();
        return;
    }
    const QString tempJpegPath = tempFile->fileName();
    tempFile->close();

    auto *process = new QProcess(this);
    m_runningProcesses.insert(job->id(), process);

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, job, process, tempJpegPath, tempFile](int exitCode, QProcess::ExitStatus exitStatus) {
                m_runningProcesses.remove(job->id());
                process->deleteLater();

                const bool reencodeOk =
                    exitStatus == QProcess::NormalExit && exitCode == 0 && QFileInfo::exists(tempJpegPath);
                if (!reencodeOk) {
                    QFile::remove(tempJpegPath);
                    tempFile->deleteLater();
                    finishJob(job, false, QStringLiteral("Could not prepare image for PDF embedding: %1")
                                              .arg(QString::fromUtf8(process->readAllStandardError())));
                    return;
                }

                QString error;
                const bool ok = PdfImageWriter::writeSingleImagePdf(tempJpegPath, job->outputPath(), &error);
                QFile::remove(tempJpegPath);
                tempFile->deleteLater();
                finishJob(job, ok, error);
            });

    process->start(QStringLiteral("ffmpeg"),
                    {QStringLiteral("-y"), QStringLiteral("-i"), job->inputPath(), tempJpegPath});
}

void PdfEngine::compressPdf(ConversionJob *job) {
    const int jpegQuality = job->parameters().value(QStringLiteral("jpegQuality"), 60).toInt();
    const QStringList args{
        QStringLiteral("--optimize-images"),
        QStringLiteral("--jpeg-quality=%1").arg(jpegQuality),
        QStringLiteral("--compress-streams=y"),
        QStringLiteral("--object-streams=generate"),
        job->inputPath(),
        job->outputPath(),
    };

    auto *process = new QProcess(this);
    m_runningProcesses.insert(job->id(), process);

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, job, process](int exitCode, QProcess::ExitStatus exitStatus) {
                // qpdf exits 3 for "warnings only" (still a usable output file).
                const bool success = (exitStatus == QProcess::NormalExit) && (exitCode == 0 || exitCode == 3) &&
                                      QFileInfo::exists(job->outputPath());
                const QString error = success ? QString() : QString::fromUtf8(process->readAllStandardError());
                m_runningProcesses.remove(job->id());
                process->deleteLater();
                finishJob(job, success, error);
            });

    process->start(QStringLiteral("qpdf"), args);
}

void PdfEngine::finishJob(ConversionJob *job, bool success, const QString &errorMessage) {
    if (success) {
        job->setProgressPercent(100);
        job->setStatus(JobStatus::Completed);
    } else {
        job->setErrorMessage(errorMessage);
        job->setStatus(JobStatus::Failed);
    }
    emit jobFinished(job->id(), success, errorMessage);
}

void PdfEngine::cancelConversion(const QUuid &jobId) {
    if (auto *process = m_runningProcesses.value(jobId)) {
        process->kill();
    }
}

} // namespace magnify::engines::pdf
