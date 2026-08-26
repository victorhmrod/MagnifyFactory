#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVariantMap>

namespace magnify::core {

enum class JobStatus {
    Queued,
    Preparing,
    Running,
    Paused,
    Completed,
    Failed,
    Cancelled
};

QString jobStatusToString(JobStatus status);

// Arbitrary conversion parameters (codec, bitrate, CRF, resolution, ...).
// Kept as a QVariantMap so engines can interpret only the keys they need
// without ConversionJob knowing about every engine-specific option.
using JobParameters = QVariantMap;

class ConversionJob : public QObject {
    Q_OBJECT
public:
    explicit ConversionJob(QString inputPath, QString outputPath, QObject *parent = nullptr);

    QUuid id() const { return m_id; }
    QString inputPath() const { return m_inputPath; }
    QString outputPath() const { return m_outputPath; }

    // Additional inputs beyond inputPath(), used only by multi-file
    // operations (currently: PdfEngine's merge). Every other job type
    // leaves this empty and engines should treat inputPath() as the sole
    // input.
    QStringList extraInputPaths() const { return m_extraInputPaths; }
    void setExtraInputPaths(const QStringList &paths) { m_extraInputPaths = paths; }

    QString sourceFormat() const { return m_sourceFormat; }
    void setSourceFormat(const QString &format) { m_sourceFormat = format; }

    QString targetFormat() const { return m_targetFormat; }
    void setTargetFormat(const QString &format) { m_targetFormat = format; }

    QString engineName() const { return m_engineName; }
    void setEngineName(const QString &name) { m_engineName = name; }

    QString presetName() const { return m_presetName; }
    void setPresetName(const QString &name) { m_presetName = name; }

    JobParameters parameters() const { return m_parameters; }
    void setParameters(const JobParameters &parameters) { m_parameters = parameters; }

    int priority() const { return m_priority; }
    void setPriority(int priority) { m_priority = priority; }

    JobStatus status() const { return m_status; }
    void setStatus(JobStatus status);

    int progressPercent() const { return m_progressPercent; }
    void setProgressPercent(int percent);

    double speedFactor() const { return m_speedFactor; }
    void setSpeedFactor(double factor) { m_speedFactor = factor; }

    qint64 etaSeconds() const { return m_etaSeconds; }
    void setEtaSeconds(qint64 seconds) { m_etaSeconds = seconds; }

    QDateTime createdAt() const { return m_createdAt; }
    QDateTime startedAt() const { return m_startedAt; }
    QDateTime completedAt() const { return m_completedAt; }

    QString errorMessage() const { return m_errorMessage; }
    void setErrorMessage(const QString &message) { m_errorMessage = message; }

signals:
    void statusChanged(JobStatus status);
    void progressChanged(int percent);

private:
    QUuid m_id;
    QString m_inputPath;
    QString m_outputPath;
    QStringList m_extraInputPaths;
    QString m_sourceFormat;
    QString m_targetFormat;
    QString m_engineName;
    QString m_presetName;
    JobParameters m_parameters;

    int m_priority = 0;
    JobStatus m_status = JobStatus::Queued;
    int m_progressPercent = 0;
    double m_speedFactor = 0.0;
    qint64 m_etaSeconds = -1;

    QDateTime m_createdAt;
    QDateTime m_startedAt;
    QDateTime m_completedAt;
    QString m_errorMessage;
};

} // namespace magnify::core
