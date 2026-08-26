#include "ConversionJob.h"

namespace magnify::core {

QString jobStatusToString(JobStatus status) {
    switch (status) {
        case JobStatus::Queued: return QStringLiteral("Queued");
        case JobStatus::Preparing: return QStringLiteral("Preparing");
        case JobStatus::Running: return QStringLiteral("Running");
        case JobStatus::Paused: return QStringLiteral("Paused");
        case JobStatus::Completed: return QStringLiteral("Completed");
        case JobStatus::Failed: return QStringLiteral("Failed");
        case JobStatus::Cancelled: return QStringLiteral("Cancelled");
    }
    return QStringLiteral("Unknown");
}

ConversionJob::ConversionJob(QString inputPath, QString outputPath, QObject *parent)
    : QObject(parent)
    , m_id(QUuid::createUuid())
    , m_inputPath(std::move(inputPath))
    , m_outputPath(std::move(outputPath))
    , m_createdAt(QDateTime::currentDateTime()) {
}

void ConversionJob::setStatus(JobStatus status) {
    if (m_status == status) {
        return;
    }
    m_status = status;
    if (status == JobStatus::Running && !m_startedAt.isValid()) {
        m_startedAt = QDateTime::currentDateTime();
    }
    if (status == JobStatus::Completed || status == JobStatus::Failed || status == JobStatus::Cancelled) {
        m_completedAt = QDateTime::currentDateTime();
    }
    emit statusChanged(status);
}

void ConversionJob::setProgressPercent(int percent) {
    const int clamped = qBound(0, percent, 100);
    if (m_progressPercent == clamped) {
        return;
    }
    m_progressPercent = clamped;
    emit progressChanged(clamped);
}

} // namespace magnify::core
