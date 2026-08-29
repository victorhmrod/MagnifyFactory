#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QProcess>
#include <QUuid>

#include "engines/IMediaEngine.h"

namespace magnify::engines::ffmpeg {

// FFmpeg-backed implementation of IMediaEngine. Owns one QProcess per running
// job (keyed by job id) so several conversions can run concurrently; each
// process is launched with `-progress pipe:1` and its stdout is parsed as
// key=value pairs, which is far more robust than scraping stderr text.
class FFmpegMediaEngine : public magnify::engines::IMediaEngine {
    Q_OBJECT
public:
    explicit FFmpegMediaEngine(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("FFmpeg"); }

    MediaProbeResult probe(const QString &filePath) override;
    void startConversion(magnify::core::ConversionJob *job) override;
    void cancelConversion(const QUuid &jobId) override;

    // Builds the ffmpeg argument list for a given job. Exposed publicly so
    // it is independently testable without spawning a real process.
    static QStringList buildArgsForJob(magnify::core::ConversionJob *job,
                                        const MediaProbeResult &probeResult);

    // Builds the ffmpeg argument list for job->parameters()["operation"] ==
    // "videoEdit": trims each clip (job->inputPath() + extraInputPaths(),
    // in order) per parameters()["clipTrims"], concatenates them, then
    // applies color/speed/text-overlay filters from the remaining
    // parameters(). A filter_complex graph, unlike the single-input, mostly
    // flag-based shape buildArgsForJob() produces — different enough to
    // warrant its own builder rather than bending FFmpegCommandBuilder
    // around a case it wasn't designed for. Also independently testable.
    static QStringList buildVideoEditArgs(magnify::core::ConversionJob *job);

    // Builds the ffmpeg argument list for job->parameters()["operation"] ==
    // "imageEdit": a single linear -vf chain (crop, then resize, then
    // color/blur, then a text overlay) applied to one input image. Far
    // simpler than buildVideoEditArgs() since there's no timeline to
    // assemble — one frame in, one frame out.
    static QStringList buildImageEditArgs(magnify::core::ConversionJob *job);

private:
    struct RunningJob {
        QProcess *process = nullptr;
        magnify::core::ConversionJob *job = nullptr;
        double totalDurationSeconds = 0.0;
        QElapsedTimer wallClock;
        QByteArray stderrBuffer;
        QByteArray progressBuffer;
    };

    void handleProgressData(const QUuid &jobId);
    void handleProcessFinished(const QUuid &jobId, int exitCode, QProcess::ExitStatus exitStatus);

    QHash<QUuid, RunningJob> m_runningJobs;
};

} // namespace magnify::engines::ffmpeg
