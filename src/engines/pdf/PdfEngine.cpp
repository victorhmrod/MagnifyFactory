#include "PdfEngine.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QUuid>
#include <memory>

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
    const QString operation = job->parameters().value(QStringLiteral("operation")).toString();

    if (sourceExt == QStringLiteral("pdf") && operation == QStringLiteral("merge")) {
        mergePdf(job);
    } else if (sourceExt == QStringLiteral("pdf") && operation == QStringLiteral("split")) {
        splitPdf(job);
    } else if (sourceExt == QStringLiteral("pdf") && targetExt == QStringLiteral("pdf")) {
        compressPdf(job);
    } else if (sourceExt == QStringLiteral("pdf")) {
        convertPdfToImage(job);
    } else if (targetExt == QStringLiteral("pdf")) {
        convertImageToPdf(job);
    } else {
        finishJob(job, false, QStringLiteral("PDF Tools cannot convert %1 to %2").arg(sourceExt, targetExt));
    }
}

void PdfEngine::runProcess(ConversionJob *job, const QString &program, const QStringList &args,
                            ProcessFinishedHandler onFinished) {
    auto *process = new QProcess(this);
    m_runningProcesses.insert(job->id(), process);

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, job, process, onFinished = std::move(onFinished)](int exitCode, QProcess::ExitStatus exitStatus) {
                m_runningProcesses.remove(job->id());
                onFinished(process, exitCode, exitStatus);
                process->deleteLater();
            });
    // Without this, a process that fails to even start (the tool missing
    // from PATH, permissions, ...) never emits finished(), and the job —
    // and anything waiting on its statusChanged signal — hangs forever
    // instead of failing with a clear message.
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

void PdfEngine::convertPdfToImage(ConversionJob *job) {
    const QString targetExt = job->targetFormat().toLower();
    const bool isJpeg = targetExt == QStringLiteral("jpg") || targetExt == QStringLiteral("jpeg");

    // pdftoppm appends its own extension to the prefix; with -singlefile (and
    // a single-page range) it writes exactly "<prefix>.<ext>" with no page
    // number suffix, which lets us target ConversionJob's single output path.
    const QFileInfo outputInfo(job->outputPath());
    const QString outputPrefix = QDir(outputInfo.absolutePath()).filePath(outputInfo.completeBaseName());

    const int dpi = job->parameters().value(QStringLiteral("dpi"), 150).toInt();
    const QStringList args{
        isJpeg ? QStringLiteral("-jpeg") : QStringLiteral("-png"),
        QStringLiteral("-singlefile"),
        QStringLiteral("-r"), QString::number(dpi),
        QStringLiteral("-f"), QStringLiteral("1"),
        QStringLiteral("-l"), QStringLiteral("1"),
        job->inputPath(),
        outputPrefix,
    };

    runProcess(job, QStringLiteral("pdftoppm"), args,
               [this, job, outputPrefix, targetExt](QProcess *process, int exitCode, QProcess::ExitStatus exitStatus) {
                   const QString producedPath = outputPrefix + QStringLiteral(".") + targetExt;
                   const bool success =
                       exitStatus == QProcess::NormalExit && exitCode == 0 && QFileInfo::exists(producedPath);
                   if (success && producedPath != job->outputPath()) {
                       QFile::remove(job->outputPath());
                       QFile::rename(producedPath, job->outputPath());
                   }
                   const QString error = success ? QString() : QString::fromUtf8(process->readAllStandardError());
                   finishJob(job, success, error);
               });
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

    runProcess(job, QStringLiteral("ffmpeg"),
               {QStringLiteral("-y"), QStringLiteral("-i"), job->inputPath(), tempJpegPath},
               [this, job, tempJpegPath, tempFile](QProcess *process, int exitCode, QProcess::ExitStatus exitStatus) {
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

    runProcess(job, QStringLiteral("qpdf"), args,
               [this, job](QProcess *process, int exitCode, QProcess::ExitStatus exitStatus) {
                   // qpdf exits 3 for "warnings only" (still a usable output file).
                   const bool success = (exitStatus == QProcess::NormalExit) && (exitCode == 0 || exitCode == 3) &&
                                         QFileInfo::exists(job->outputPath());
                   const QString error = success ? QString() : QString::fromUtf8(process->readAllStandardError());
                   finishJob(job, success, error);
               });
}

void PdfEngine::mergePdf(ConversionJob *job) {
    auto inputs = std::make_shared<QStringList>();
    *inputs << job->inputPath();
    *inputs << job->extraInputPaths();
    auto tempFiles = std::make_shared<QStringList>();
    convertNextMergeInput(job, inputs, 0, tempFiles);
}

void PdfEngine::convertNextMergeInput(ConversionJob *job, std::shared_ptr<QStringList> inputs, int index,
                                       std::shared_ptr<QStringList> tempFiles) {
    if (index >= inputs->size()) {
        runMergeQpdf(job, inputs, tempFiles);
        return;
    }

    const QString path = inputs->at(index);
    if (path.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)) {
        convertNextMergeInput(job, inputs, index + 1, tempFiles);
        return;
    }

    // Non-PDF entry: an image being merged alongside PDFs (or with other
    // images) — turn it into a single-page temp PDF first, same embedding
    // path convertImageToPdf() uses (JPEG embeds directly; anything else is
    // re-encoded to JPEG via ffmpeg first), then substitute it in place and
    // move on to the next input.
    const QString tempPdfPath =
        QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
            .filePath(QStringLiteral("magnify_merge_%1.pdf").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));

    auto embedAndContinue = [this, job, inputs, index, tempFiles, tempPdfPath](const QString &jpegPath) {
        QString error;
        const bool ok = PdfImageWriter::writeSingleImagePdf(jpegPath, tempPdfPath, &error);
        if (!ok) {
            finishJob(job, false,
                      QStringLiteral("Could not prepare %1 for merging: %2").arg(inputs->at(index), error));
            return;
        }
        (*inputs)[index] = tempPdfPath;
        tempFiles->append(tempPdfPath);
        convertNextMergeInput(job, inputs, index + 1, tempFiles);
    };

    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QStringLiteral("jpg") || ext == QStringLiteral("jpeg")) {
        embedAndContinue(path);
        return;
    }

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

    runProcess(job, QStringLiteral("ffmpeg"), {QStringLiteral("-y"), QStringLiteral("-i"), path, tempJpegPath},
               [this, job, tempJpegPath, tempFile, embedAndContinue](QProcess *process, int exitCode,
                                                                       QProcess::ExitStatus exitStatus) {
                   const bool reencodeOk =
                       exitStatus == QProcess::NormalExit && exitCode == 0 && QFileInfo::exists(tempJpegPath);
                   if (!reencodeOk) {
                       QFile::remove(tempJpegPath);
                       tempFile->deleteLater();
                       finishJob(job, false, QStringLiteral("Could not prepare image for merging: %1")
                                                 .arg(QString::fromUtf8(process->readAllStandardError())));
                       return;
                   }
                   embedAndContinue(tempJpegPath);
                   QFile::remove(tempJpegPath);
                   tempFile->deleteLater();
               });
}

void PdfEngine::runMergeQpdf(ConversionJob *job, std::shared_ptr<QStringList> inputs,
                              std::shared_ptr<QStringList> tempFiles) {
    // `qpdf --empty --pages A B C -- out.pdf` concatenates every page of
    // A, B, C (in order) into out.pdf; --empty is a required dummy "base"
    // document since --pages is otherwise meant to select from an existing
    // one. Omitting a page range after each filename defaults to "all pages".
    QStringList args{QStringLiteral("--empty"), QStringLiteral("--pages")};
    args << *inputs;
    args << QStringLiteral("--") << job->outputPath();

    runProcess(job, QStringLiteral("qpdf"), args,
               [this, job, tempFiles](QProcess *process, int exitCode, QProcess::ExitStatus exitStatus) {
                   for (const QString &temp : *tempFiles) {
                       QFile::remove(temp);
                   }
                   const bool success = (exitStatus == QProcess::NormalExit) && (exitCode == 0 || exitCode == 3) &&
                                        QFileInfo::exists(job->outputPath());
                   const QString error = success ? QString() : QString::fromUtf8(process->readAllStandardError());
                   finishJob(job, success, error);
               });
}

void PdfEngine::splitPdf(ConversionJob *job) {
    // job->outputPath() is expected to contain a "%d" page-number
    // placeholder (MainWindow builds it as "<name>-page-%d.pdf"); qpdf fills
    // it in per output file and pads the digits to the source's page count.
    const QStringList args{QStringLiteral("--split-pages"), job->inputPath(), job->outputPath()};

    runProcess(job, QStringLiteral("qpdf"), args,
               [this, job](QProcess *process, int exitCode, QProcess::ExitStatus exitStatus) {
                   const bool success = (exitStatus == QProcess::NormalExit) && (exitCode == 0 || exitCode == 3);
                   const QString error = success ? QString() : QString::fromUtf8(process->readAllStandardError());
                   finishJob(job, success, error);
               });
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
