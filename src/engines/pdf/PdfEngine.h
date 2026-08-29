#pragma once

#include <QHash>
#include <QProcess>
#include <functional>
#include <memory>

#include "engines/IMediaEngine.h"

namespace magnify::engines::pdf {

// PDF <-> image conversion, backed by the Poppler command-line tools
// (pdftoppm for rendering). Image -> PDF is handled in-process by
// PdfImageWriter (with FFmpeg used only to pre-convert non-JPEG sources),
// since Poppler has no PDF-writing tool. PDF -> PDF is a qpdf pass whose
// exact behavior is chosen by job->parameters()["operation"]: "merge"
// (job->inputPath() + extraInputPaths(), all pages, concatenated — image
// inputs are transparently turned into single-page PDFs first, so a merge
// can freely mix PDFs and images into one output), "split" (one output
// file per page), or the default, compress.
class PdfEngine : public magnify::engines::IMediaEngine {
    Q_OBJECT
public:
    explicit PdfEngine(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("PDF Tools"); }

    MediaProbeResult probe(const QString &filePath) override;
    void startConversion(magnify::core::ConversionJob *job) override;
    void cancelConversion(const QUuid &jobId) override;

    // Synchronous (`qpdf --show-npages`); used by the Document Editor UI to
    // size its page grid before any thumbnails are rendered. Returns -1 on
    // failure (missing/invalid file, qpdf not installed).
    static int pageCount(const QString &filePath);

private:
    void convertPdfToImage(magnify::core::ConversionJob *job);
    void convertImageToPdf(magnify::core::ConversionJob *job);
    void compressPdf(magnify::core::ConversionJob *job);
    void mergePdf(magnify::core::ConversionJob *job);
    // Recursively turns each non-PDF entry of *inputs into a temp
    // single-page PDF (in place) before handing off to runMergeQpdf();
    // pure-PDF merges walk straight through with nothing to convert.
    void convertNextMergeInput(magnify::core::ConversionJob *job, std::shared_ptr<QStringList> inputs, int index,
                                std::shared_ptr<QStringList> tempFiles);
    void runMergeQpdf(magnify::core::ConversionJob *job, std::shared_ptr<QStringList> inputs,
                       std::shared_ptr<QStringList> tempFiles);
    void splitPdf(magnify::core::ConversionJob *job);
    // Rebuilds a PDF from job->parameters()["pages"] (a per-output-page list
    // of {"source": 0=inputPath()/1+=extraInputPaths()[source-1], "page":
    // 1-based page number in that source}), then applies
    // job->parameters()["rotations"] (final 1-based page number -> absolute
    // angle 90/180/270) as a second qpdf pass. Lets the Document Editor
    // reorder, delete, duplicate, insert pages from other PDFs, and rotate
    // pages, all via real qpdf processing on export.
    void documentEditPdf(magnify::core::ConversionJob *job);
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
