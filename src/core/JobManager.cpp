#include "JobManager.h"

namespace magnify::core {

JobManager::JobManager(QObject *parent) : QObject(parent) {
}

void JobManager::registerEngine(magnify::engines::IMediaEngine *engine) {
    m_engines.insert(engine->name(), engine);
    connect(engine, &magnify::engines::IMediaEngine::jobFinished, this, &JobManager::onEngineJobFinished);
}

magnify::engines::IMediaEngine *JobManager::engineForJob(ConversionJob *job) const {
    return m_engines.value(job->engineName(), nullptr);
}

ConversionJob *JobManager::addJob(std::unique_ptr<ConversionJob> job) {
    ConversionJob *raw = job.release();
    raw->setParent(this);
    m_jobs.append(raw);
    emit jobAdded(raw);
    emit queueChanged();
    if (m_queueRunning) {
        tryStartNextJobs();
    }
    return raw;
}

void JobManager::removeJob(const QUuid &jobId) {
    for (int i = 0; i < m_jobs.size(); ++i) {
        if (m_jobs[i]->id() == jobId) {
            ConversionJob *job = m_jobs.takeAt(i);
            if (job->status() == JobStatus::Running) {
                if (auto *engine = engineForJob(job)) {
                    engine->cancelConversion(jobId);
                }
            }
            job->deleteLater();
            emit jobRemoved(jobId);
            emit queueChanged();
            return;
        }
    }
}

void JobManager::cancelJob(const QUuid &jobId) {
    if (ConversionJob *job = findJob(jobId)) {
        if (job->status() == JobStatus::Running) {
            if (auto *engine = engineForJob(job)) {
                engine->cancelConversion(jobId);
            }
        } else if (job->status() == JobStatus::Queued) {
            job->setStatus(JobStatus::Cancelled);
        }
    }
}

void JobManager::retryJob(const QUuid &jobId) {
    if (ConversionJob *job = findJob(jobId)) {
        if (job->status() == JobStatus::Failed || job->status() == JobStatus::Cancelled) {
            job->setProgressPercent(0);
            job->setErrorMessage(QString());
            job->setStatus(JobStatus::Queued);
            if (m_queueRunning) {
                tryStartNextJobs();
            }
        }
    }
}

ConversionJob *JobManager::duplicateJob(const QUuid &jobId) {
    ConversionJob *source = findJob(jobId);
    if (!source) {
        return nullptr;
    }
    auto copy = std::make_unique<ConversionJob>(source->inputPath(), source->outputPath());
    copy->setSourceFormat(source->sourceFormat());
    copy->setTargetFormat(source->targetFormat());
    copy->setEngineName(source->engineName());
    copy->setPresetName(source->presetName());
    copy->setParameters(source->parameters());
    copy->setPriority(source->priority());
    return addJob(std::move(copy));
}

void JobManager::setMaxConcurrentJobs(int max) {
    m_maxConcurrentJobs = qMax(1, max);
    if (m_queueRunning) {
        tryStartNextJobs();
    }
}

void JobManager::startQueue() {
    m_queueRunning = true;
    tryStartNextJobs();
}

void JobManager::pauseQueue() {
    m_queueRunning = false;
}

ConversionJob *JobManager::findJob(const QUuid &jobId) const {
    for (ConversionJob *job : m_jobs) {
        if (job->id() == jobId) {
            return job;
        }
    }
    return nullptr;
}

int JobManager::runningJobCount() const {
    int count = 0;
    for (ConversionJob *job : m_jobs) {
        if (job->status() == JobStatus::Running || job->status() == JobStatus::Preparing) {
            ++count;
        }
    }
    return count;
}

void JobManager::tryStartNextJobs() {
    if (!m_queueRunning) {
        return;
    }
    for (ConversionJob *job : m_jobs) {
        if (runningJobCount() >= m_maxConcurrentJobs) {
            break;
        }
        if (job->status() == JobStatus::Queued) {
            if (auto *engine = engineForJob(job)) {
                engine->startConversion(job);
            } else {
                job->setErrorMessage(QStringLiteral("No engine registered for '%1'").arg(job->engineName()));
                job->setStatus(JobStatus::Failed);
            }
        }
    }
}

void JobManager::onEngineJobFinished(QUuid /*jobId*/, bool /*success*/, QString /*errorMessage*/) {
    emit queueChanged();
    tryStartNextJobs();
}

} // namespace magnify::core
