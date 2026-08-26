#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

#include "core/ConversionJob.h"

namespace magnify::engines {

// Metadata describing a probed media file, filled in by IMediaEngine::probe().
// Mirrors what ffprobe/MediaInfo would report.
struct MediaProbeResult {
    bool valid = false;
    QString container;
    QString videoCodec;
    QString audioCodec;
    int width = 0;
    int height = 0;
    double frameRate = 0.0;
    double durationSeconds = 0.0;
    qint64 bitrateBps = 0;
    int audioSampleRate = 0;
    int audioChannels = 0;
    qint64 fileSizeBytes = 0;
    QString errorMessage;
};

// Abstraction over any backend capable of transcoding/remuxing media
// (FFmpeg today; other backends can implement the same contract later).
class IMediaEngine : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~IMediaEngine() override = default;

    virtual QString name() const = 0;

    // Synchronous probe — fast, intended for populating a media inspector view.
    virtual MediaProbeResult probe(const QString &filePath) = 0;

    // Starts an asynchronous conversion for the given job. Progress/completion
    // are reported through the signals below, keyed by job id so a single
    // engine instance can be shared by JobManager across concurrent jobs.
    virtual void startConversion(magnify::core::ConversionJob *job) = 0;
    virtual void cancelConversion(const QUuid &jobId) = 0;

signals:
    void jobProgress(QUuid jobId, int percent, double speedFactor, qint64 etaSeconds);
    void jobFinished(QUuid jobId, bool success, QString errorMessage);
};

} // namespace magnify::engines
