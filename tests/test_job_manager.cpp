// Exercises JobManager's queue-control logic (pause/resume/retry/cancel/
// reorder) in isolation, with no engine registered and the queue never
// started, so nothing ever leaves the Queued state on its own. Part of
// ctest — no external process or real conversion involved.
#include <QTest>

#include "core/ConversionJob.h"
#include "core/JobManager.h"

using magnify::core::ConversionJob;
using magnify::core::JobManager;
using magnify::core::JobStatus;

class TestJobManager : public QObject {
    Q_OBJECT
private slots:
    void pauseThenResumeReturnsJobToQueued() {
        JobManager manager;
        auto job = std::make_unique<ConversionJob>("in.mp4", "out.mp3");
        const QUuid id = manager.addJob(std::move(job))->id();

        manager.pauseJob(id);
        QCOMPARE(manager.findJob(id)->status(), JobStatus::Paused);

        manager.resumeJob(id);
        QCOMPARE(manager.findJob(id)->status(), JobStatus::Queued);
    }

    void pauseIgnoredForNonQueuedJob() {
        JobManager manager;
        auto job = std::make_unique<ConversionJob>("in.mp4", "out.mp3");
        ConversionJob *raw = manager.addJob(std::move(job));
        raw->setStatus(JobStatus::Completed);

        manager.pauseJob(raw->id());
        QCOMPARE(raw->status(), JobStatus::Completed);
    }

    void retryResetsFailedJobToQueued() {
        JobManager manager;
        auto job = std::make_unique<ConversionJob>("in.mp4", "out.mp3");
        ConversionJob *raw = manager.addJob(std::move(job));
        raw->setErrorMessage("boom");
        raw->setStatus(JobStatus::Failed);

        manager.retryJob(raw->id());
        QCOMPARE(raw->status(), JobStatus::Queued);
        QVERIFY(raw->errorMessage().isEmpty());
    }

    void cancelPausedJobMarksItCancelled() {
        JobManager manager;
        auto job = std::make_unique<ConversionJob>("in.mp4", "out.mp3");
        const QUuid id = manager.addJob(std::move(job))->id();
        manager.pauseJob(id);

        manager.cancelJob(id);
        QCOMPARE(manager.findJob(id)->status(), JobStatus::Cancelled);
    }

    void setJobOrderReordersJobsList() {
        JobManager manager;
        const QUuid id1 = manager.addJob(std::make_unique<ConversionJob>("a.mp4", "a.mp3"))->id();
        const QUuid id2 = manager.addJob(std::make_unique<ConversionJob>("b.mp4", "b.mp3"))->id();
        const QUuid id3 = manager.addJob(std::make_unique<ConversionJob>("c.mp4", "c.mp3"))->id();

        manager.setJobOrder({id3, id1, id2});

        QCOMPARE(manager.jobs().size(), 3);
        QCOMPARE(manager.jobs().at(0)->id(), id3);
        QCOMPARE(manager.jobs().at(1)->id(), id1);
        QCOMPARE(manager.jobs().at(2)->id(), id2);
    }

    void removeJobDropsItFromTheList() {
        JobManager manager;
        const QUuid id = manager.addJob(std::make_unique<ConversionJob>("in.mp4", "out.mp3"))->id();
        QCOMPARE(manager.jobs().size(), 1);

        manager.removeJob(id);
        QCOMPARE(manager.jobs().size(), 0);
        QVERIFY(manager.findJob(id) == nullptr);
    }
};

QTEST_APPLESS_MAIN(TestJobManager)
#include "test_job_manager.moc"
