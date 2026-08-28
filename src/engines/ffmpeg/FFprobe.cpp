#include "FFprobe.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

#include "core/HostProcess.h"

namespace magnify::engines::ffmpeg {

MediaProbeResult FFprobe::probe(const QString &filePath) {
    MediaProbeResult result;

    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        result.errorMessage = QStringLiteral("File does not exist: %1").arg(filePath);
        return result;
    }
    result.fileSizeBytes = info.size();

    QProcess process;
    const QStringList args{
        QStringLiteral("-v"), QStringLiteral("quiet"),
        QStringLiteral("-print_format"), QStringLiteral("json"),
        QStringLiteral("-show_format"),
        QStringLiteral("-show_streams"),
        filePath,
    };
    magnify::core::HostProcess::start(&process, QStringLiteral("ffprobe"), args);
    if (!process.waitForStarted(5000)) {
        result.errorMessage = QStringLiteral("Could not start ffprobe. Is FFmpeg installed and on PATH?");
        return result;
    }
    if (!process.waitForFinished(30000)) {
        process.kill();
        result.errorMessage = QStringLiteral("ffprobe timed out while probing the file.");
        return result;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        result.errorMessage = QStringLiteral("ffprobe exited with an error: %1")
                                   .arg(QString::fromUtf8(process.readAllStandardError()));
        return result;
    }

    return parseJson(process.readAllStandardOutput());
}

MediaProbeResult FFprobe::parseJson(const QByteArray &json) {
    MediaProbeResult result;

    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        result.errorMessage = QStringLiteral("ffprobe returned invalid JSON.");
        return result;
    }

    const QJsonObject root = doc.object();
    const QJsonObject format = root.value(QStringLiteral("format")).toObject();

    result.container = format.value(QStringLiteral("format_name")).toString();
    result.durationSeconds = format.value(QStringLiteral("duration")).toString().toDouble();
    result.bitrateBps = format.value(QStringLiteral("bit_rate")).toString().toLongLong();

    const QJsonArray streams = root.value(QStringLiteral("streams")).toArray();
    for (const auto &streamValue : streams) {
        const QJsonObject stream = streamValue.toObject();
        const QString codecType = stream.value(QStringLiteral("codec_type")).toString();

        if (codecType == QStringLiteral("video") && result.videoCodec.isEmpty()) {
            result.videoCodec = stream.value(QStringLiteral("codec_name")).toString();
            result.width = stream.value(QStringLiteral("width")).toInt();
            result.height = stream.value(QStringLiteral("height")).toInt();

            const QString rFrameRate = stream.value(QStringLiteral("r_frame_rate")).toString();
            const QStringList parts = rFrameRate.split('/');
            if (parts.size() == 2 && parts[1].toDouble() != 0.0) {
                result.frameRate = parts[0].toDouble() / parts[1].toDouble();
            }
        } else if (codecType == QStringLiteral("audio") && result.audioCodec.isEmpty()) {
            result.audioCodec = stream.value(QStringLiteral("codec_name")).toString();
            result.audioSampleRate = stream.value(QStringLiteral("sample_rate")).toString().toInt();
            result.audioChannels = stream.value(QStringLiteral("channels")).toInt();
        }
    }

    result.valid = true;
    return result;
}

} // namespace magnify::engines::ffmpeg
