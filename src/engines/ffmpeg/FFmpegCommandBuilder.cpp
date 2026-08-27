#include "FFmpegCommandBuilder.h"

namespace magnify::engines::ffmpeg {

FFmpegCommandBuilder &FFmpegCommandBuilder::setInput(const QString &path) {
    m_input = path;
    return *this;
}

FFmpegCommandBuilder &FFmpegCommandBuilder::setOutput(const QString &path) {
    m_output = path;
    return *this;
}

FFmpegCommandBuilder &FFmpegCommandBuilder::setOverwrite(bool overwrite) {
    m_overwrite = overwrite;
    return *this;
}

FFmpegCommandBuilder &FFmpegCommandBuilder::setVideoCodec(const QString &codec) {
    m_videoCodec = codec;
    m_copyVideo = false;
    return *this;
}

FFmpegCommandBuilder &FFmpegCommandBuilder::setAudioCodec(const QString &codec) {
    m_audioCodec = codec;
    m_copyAudio = false;
    return *this;
}

FFmpegCommandBuilder &FFmpegCommandBuilder::copyVideoStream() {
    m_copyVideo = true;
    m_videoCodec.reset();
    return *this;
}

FFmpegCommandBuilder &FFmpegCommandBuilder::copyAudioStream() {
    m_copyAudio = true;
    m_audioCodec.reset();
    return *this;
}

FFmpegCommandBuilder &FFmpegCommandBuilder::dropVideoStream() {
    m_dropVideo = true;
    return *this;
}

FFmpegCommandBuilder &FFmpegCommandBuilder::dropAudioStream() {
    m_dropAudio = true;
    return *this;
}

FFmpegCommandBuilder &FFmpegCommandBuilder::setTrim(double startSeconds, std::optional<double> endSeconds) {
    m_trimStart = startSeconds;
    m_trimEnd = endSeconds;
    return *this;
}

FFmpegCommandBuilder &FFmpegCommandBuilder::setResolution(int width, int height) {
    m_width = width;
    m_height = height;
    return *this;
}

FFmpegCommandBuilder &FFmpegCommandBuilder::setFrameRate(double fps) {
    m_frameRate = fps;
    return *this;
}

FFmpegCommandBuilder &FFmpegCommandBuilder::setVideoBitrate(const QString &bitrate) {
    m_videoBitrate = bitrate;
    return *this;
}

FFmpegCommandBuilder &FFmpegCommandBuilder::setAudioBitrate(const QString &bitrate) {
    m_audioBitrate = bitrate;
    return *this;
}

FFmpegCommandBuilder &FFmpegCommandBuilder::setCrf(int crf) {
    m_crf = crf;
    return *this;
}

FFmpegCommandBuilder &FFmpegCommandBuilder::setAudioQuality(int qscale) {
    m_audioQuality = qscale;
    return *this;
}

FFmpegCommandBuilder &FFmpegCommandBuilder::setSampleRate(int hz) {
    m_sampleRate = hz;
    return *this;
}

FFmpegCommandBuilder &FFmpegCommandBuilder::setAudioChannels(int channels) {
    m_audioChannels = channels;
    return *this;
}

FFmpegCommandBuilder &FFmpegCommandBuilder::setHardwareEncoder(const QString &hwEncoderName) {
    m_hardwareEncoder = hwEncoderName;
    return *this;
}

FFmpegCommandBuilder &FFmpegCommandBuilder::setPixelFormat(const QString &pixFmt) {
    m_pixelFormat = pixFmt;
    return *this;
}

FFmpegCommandBuilder &FFmpegCommandBuilder::addVideoFilter(const QString &filterExpr) {
    m_videoFilters << filterExpr;
    return *this;
}

FFmpegCommandBuilder &FFmpegCommandBuilder::addAudioFilter(const QString &filterExpr) {
    m_audioFilters << filterExpr;
    return *this;
}

FFmpegCommandBuilder &FFmpegCommandBuilder::addExtraArgs(const QStringList &args) {
    m_extraArgs << args;
    return *this;
}

FFmpegCommandBuilder &FFmpegCommandBuilder::enableProgressReporting() {
    m_progressReporting = true;
    return *this;
}

QStringList FFmpegCommandBuilder::build() const {
    QStringList args;

    args << QStringLiteral("-y") << QStringLiteral("-hide_banner");
    if (!m_overwrite) {
        args.removeAll(QStringLiteral("-y"));
        args << QStringLiteral("-n");
    }

    args << QStringLiteral("-i") << m_input;

    if (m_trimStart) {
        args << QStringLiteral("-ss") << QString::number(*m_trimStart, 'f', 3);
    }
    if (m_trimEnd) {
        args << QStringLiteral("-to") << QString::number(*m_trimEnd, 'f', 3);
    }

    if (m_hardwareEncoder) {
        // Hardware encoders (h264_nvenc, hevc_nvenc, h264_amf, h264_qsv, ...)
        // are selected as the video codec directly.
        args << QStringLiteral("-c:v") << *m_hardwareEncoder;
    } else if (m_dropVideo) {
        args << QStringLiteral("-vn");
    } else if (m_copyVideo) {
        args << QStringLiteral("-c:v") << QStringLiteral("copy");
    } else if (m_videoCodec) {
        args << QStringLiteral("-c:v") << *m_videoCodec;
    }

    if (m_dropAudio) {
        args << QStringLiteral("-an");
    } else if (m_copyAudio) {
        args << QStringLiteral("-c:a") << QStringLiteral("copy");
    } else if (m_audioCodec) {
        args << QStringLiteral("-c:a") << *m_audioCodec;
    }

    if (m_frameRate) {
        args << QStringLiteral("-r") << QString::number(*m_frameRate, 'f', 3);
    }
    if (m_videoBitrate) {
        args << QStringLiteral("-b:v") << *m_videoBitrate;
    }
    if (m_crf) {
        args << QStringLiteral("-crf") << QString::number(*m_crf);
    }
    if (m_pixelFormat) {
        args << QStringLiteral("-pix_fmt") << *m_pixelFormat;
    }

    if (m_audioBitrate) {
        args << QStringLiteral("-b:a") << *m_audioBitrate;
    }
    if (m_audioQuality) {
        args << QStringLiteral("-q:a") << QString::number(*m_audioQuality);
    }
    if (m_sampleRate) {
        args << QStringLiteral("-ar") << QString::number(*m_sampleRate);
    }
    if (m_audioChannels) {
        args << QStringLiteral("-ac") << QString::number(*m_audioChannels);
    }

    // Scale (from setResolution) and any addVideoFilter() calls must share a
    // single -vf — ffmpeg only honors the last -vf flag it sees, so emitting
    // two would silently drop whichever came first.
    QStringList videoFilters;
    if (m_width && m_height) {
        // lanczos is noticeably sharper than ffmpeg's bilinear default,
        // which matters most when upscaling (a preset asking for a bigger
        // size than the source, e.g. 480p -> 1080p).
        videoFilters << QStringLiteral("scale=%1:%2:flags=lanczos").arg(*m_width).arg(*m_height);
    }
    videoFilters << m_videoFilters;
    if (!videoFilters.isEmpty()) {
        args << QStringLiteral("-vf") << videoFilters.join(',');
    }
    if (!m_audioFilters.isEmpty()) {
        args << QStringLiteral("-af") << m_audioFilters.join(',');
    }

    args << m_extraArgs;

    if (m_progressReporting) {
        args << QStringLiteral("-progress") << QStringLiteral("pipe:1") << QStringLiteral("-nostats");
    }

    args << m_output;
    return args;
}

} // namespace magnify::engines::ffmpeg
