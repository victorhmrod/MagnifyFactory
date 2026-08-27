#pragma once

#include <QHash>
#include <QProcess>
#include <functional>

#include "engines/IMediaEngine.h"

namespace magnify::engines::document {

// Document conversion (docx/xlsx/pptx/odt/ods/odp/rtf/txt <-> PDF and
// between each other), backed by LibreOffice's headless CLI
// (`soffice --headless --convert-to <ext> --outdir <dir> <input>`).
// LibreOffice picks the output filename itself (source basename + target
// extension); the process handler renames it to job->outputPath() if the
// two differ, mirroring PdfEngine's pdftoppm rename dance.
class DocumentEngine : public magnify::engines::IMediaEngine {
    Q_OBJECT
public:
    explicit DocumentEngine(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("Document Tools"); }

    MediaProbeResult probe(const QString &filePath) override;
    void startConversion(magnify::core::ConversionJob *job) override;
    void cancelConversion(const QUuid &jobId) override;

    // Looks for soffice.exe on PATH, then common install locations.
    static QString sofficeExecutable();

private:
    void finishJob(magnify::core::ConversionJob *job, bool success, const QString &errorMessage);

    QHash<QUuid, QProcess *> m_runningProcesses;
};

} // namespace magnify::engines::document
