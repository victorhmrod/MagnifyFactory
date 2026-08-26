#pragma once

#include <QHash>
#include <QProcess>
#include <functional>

#include "engines/IMediaEngine.h"

namespace magnify::engines::pdf {

// PDF <-> image conversion, backed by the Poppler command-line tools
// (pdftoppm for rendering). Image -> PDF is handled in-process by
// PdfImageWriter (with FFmpeg used only to pre-convert non-JPEG sources),
// since Poppler has no PDF-writing tool. PDF -> PDF is a qpdf pass whose
// exact behavior is chosen by job->parameters()["operation"]: "merge"
// (job->inputPath() + extraInputPaths(), all pages, concatenated),
// "split" (one output file per page), or the default, compress.
class PdfEngine : public magnify::engines::IMediaEngine {
    Q_OBJECT
public:
    explicit PdfEngine(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("PDF Tools"); }

    MediaProbeResult probe(const QString &filePath) override;
    void startConversion(magnify::core::ConversionJob *job) override;
    void cancelConversion(const QUuid &jobId) override;

private:
    void convertPdfToImage(magnify::core::ConversionJob *job);
    void convertImageToPdf(magnify::core::ConversionJob *job);
    void compressPdf(magnify::core::ConversionJob *job);
    void mergePdf(magnify::core::ConversionJob *job);
    void splitPdf(magnify::core::ConversionJob *job);
    void finishJob(magnify::core::ConversionJob *job, bool success, const QString &errorMessage);

    // Shared process bookkeeping (registering in m_runningProcesses so
    // cancelConversion() can find it, cleanup on exit) for every method that
    // shells out to pdftoppm/ffmpeg/qpdf. `onFinished` only needs to decide
    // success/failure from the exit code and produce an error message.
    using ProcessFinishedHandler = std::function<void(QProcess *process, int exitCode, QProcess::ExitStatus status)>;
    void runProcess(magnify::core::ConversionJob *job, const QString &program, const QStringList &args,
                     ProcessFinishedHandler onFinished);

    QHash<QUuid, QProcess *> m_runningProcesses;
};

} // namespace magnify::engines::pdf
