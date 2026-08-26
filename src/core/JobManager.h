#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <memory>

#include "ConversionJob.h"
#include "engines/IMediaEngine.h"

namespace magnify::core {

// Owns the conversion queue and drives it against one or more IMediaEngine
// instances (dispatched by ConversionJob::engineName()), respecting a
// configurable concurrency limit. UI code talks only to JobManager — never
// to an engine directly — keeping conversion logic out of widgets.
class JobManager : public QObject {
    Q_OBJECT
public:
    explicit JobManager(QObject *parent = nullptr);

    // Engines must be registered before jobs targeting them are added; the
    // name must match ConversionJob::engineName() for jobs routed to it.
    void registerEngine(magnify::engines::IMediaEngine *engine);

    ConversionJob *addJob(std::unique_ptr<ConversionJob> job);
    void removeJob(const QUuid &jobId);
    void cancelJob(const QUuid &jobId);
    void retryJob(const QUuid &jobId);
    ConversionJob *duplicateJob(const QUuid &jobId);

    void setMaxConcurrentJobs(int max);
    int maxConcurrentJobs() const { return m_maxConcurrentJobs; }

    void startQueue();
    void pauseQueue();

    const QList<ConversionJob *> &jobs() const { return m_jobs; }
    ConversionJob *findJob(const QUuid &jobId) const;

signals:
    void jobAdded(magnify::core::ConversionJob *job);
    void jobRemoved(QUuid jobId);
    void queueChanged();

private:
    void tryStartNextJobs();
    void onEngineJobFinished(QUuid jobId, bool success, QString errorMessage);
    int runningJobCount() const;
    magnify::engines::IMediaEngine *engineForJob(ConversionJob *job) const;

    QHash<QString, magnify::engines::IMediaEngine *> m_engines;
    QList<ConversionJob *> m_jobs; // owned via QObject parent-child ownership
    int m_maxConcurrentJobs = 2;
    bool m_queueRunning = false;
};

} // namespace magnify::core
