#pragma once

#include <QHash>
#include <QProcess>

#include "engines/IMediaEngine.h"

namespace magnify::engines::pdf {

// PDF <-> image conversion, backed by the Poppler command-line tools
// (pdftoppm for rendering). Image -> PDF is handled in-process by
// PdfImageWriter (with FFmpeg used only to pre-convert non-JPEG sources),
// since Poppler has no PDF-writing tool. PDF -> PDF is treated as a
// compression pass via the qpdf CLI.
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
    void finishJob(magnify::core::ConversionJob *job, bool success, const QString &errorMessage);

    QHash<QUuid, QProcess *> m_runningProcesses;
};

} // namespace magnify::engines::pdf
