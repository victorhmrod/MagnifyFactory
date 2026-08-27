#pragma once

#include <QHash>
#include <QProcess>

#include "engines/IMediaEngine.h"

namespace magnify::engines::archive {

// Archive extraction/creation via the 7-Zip CLI (7z.exe). Like PdfEngine's
// merge/split, this doesn't fit a simple "one input format -> one output
// format" shape, so job->parameters()["operation"] selects the behavior:
// "extract" (job->inputPath() is the archive, job->outputPath() is the
// destination folder) or "create" (job->inputPath() + extraInputPaths() are
// the files to add, job->outputPath() is the archive to produce).
class ArchiveEngine : public magnify::engines::IMediaEngine {
    Q_OBJECT
public:
    explicit ArchiveEngine(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("Archive Tools"); }

    MediaProbeResult probe(const QString &filePath) override;
    void startConversion(magnify::core::ConversionJob *job) override;
    void cancelConversion(const QUuid &jobId) override;

private:
    void extractArchive(magnify::core::ConversionJob *job);
    void createArchive(magnify::core::ConversionJob *job);
    void finishJob(magnify::core::ConversionJob *job, bool success, const QString &errorMessage);

    QHash<QUuid, QProcess *> m_runningProcesses;
};

} // namespace magnify::engines::archive
