#pragma once

#include <QStringList>
#include <QString>
#include <optional>

namespace magnify::engines::ffmpeg {

// Builds ffmpeg argument lists as a QStringList (one argv entry per element),
// never through string concatenation, so the result is safe to pass directly
// to QProcess::start()/setArguments() with no shell involved.
class FFmpegCommandBuilder {
public:
    FFmpegCommandBuilder &setInput(const QString &path);
    FFmpegCommandBuilder &setOutput(const QString &path);
    FFmpegCommandBuilder &setOverwrite(bool overwrite = true);

    FFmpegCommandBuilder &setVideoCodec(const QString &codec);
    FFmpegCommandBuilder &setAudioCodec(const QString &codec);
    FFmpegCommandBuilder &copyVideoStream();
    FFmpegCommandBuilder &copyAudioStream();
    FFmpegCommandBuilder &dropVideoStream();  // -vn, used for audio extraction
    FFmpegCommandBuilder &dropAudioStream();  // -an

    FFmpegCommandBuilder &setResolution(int width, int height);
    FFmpegCommandBuilder &setFrameRate(double fps);
    FFmpegCommandBuilder &setVideoBitrate(const QString &bitrate); // e.g. "4000k"
    FFmpegCommandBuilder &setAudioBitrate(const QString &bitrate); // e.g. "192k"
    FFmpegCommandBuilder &setCrf(int crf);
    FFmpegCommandBuilder &setAudioQuality(int qscale); // libmp3lame -q:a
    FFmpegCommandBuilder &setSampleRate(int hz);
    FFmpegCommandBuilder &setAudioChannels(int channels);
    FFmpegCommandBuilder &setHardwareEncoder(const QString &hwEncoderName);
    FFmpegCommandBuilder &setPixelFormat(const QString &pixFmt);

    FFmpegCommandBuilder &addVideoFilter(const QString &filterExpr);
    FFmpegCommandBuilder &addAudioFilter(const QString &filterExpr);
    FFmpegCommandBuilder &addExtraArgs(const QStringList &args);

    // Enables machine-readable progress on stdout ("-progress pipe:1 -nostats").
    FFmpegCommandBuilder &enableProgressReporting();

    QStringList build() const;

private:
    QString m_input;
    QString m_output;
    bool m_overwrite = true;

    std::optional<QString> m_videoCodec;
    std::optional<QString> m_audioCodec;
    bool m_copyVideo = false;
    bool m_copyAudio = false;
    bool m_dropVideo = false;
    bool m_dropAudio = false;

    std::optional<int> m_width;
    std::optional<int> m_height;
    std::optional<double> m_frameRate;
    std::optional<QString> m_videoBitrate;
    std::optional<QString> m_audioBitrate;
    std::optional<int> m_crf;
    std::optional<int> m_audioQuality;
    std::optional<int> m_sampleRate;
    std::optional<int> m_audioChannels;
    std::optional<QString> m_hardwareEncoder;
    std::optional<QString> m_pixelFormat;

    QStringList m_videoFilters;
    QStringList m_audioFilters;
    QStringList m_extraArgs;
    bool m_progressReporting = false;
};

} // namespace magnify::engines::ffmpeg
