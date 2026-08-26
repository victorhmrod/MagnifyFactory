#pragma once

#include "engines/IMediaEngine.h"

namespace magnify::engines::ffmpeg {

// Thin wrapper around the `ffprobe` executable. Runs synchronously (probing
// is fast and the UI needs the result immediately after a drag-and-drop),
// using QProcess — never a shell — with `-print_format json` so parsing is
// done through QJsonDocument instead of ad-hoc text scraping.
class FFprobe {
public:
    static MediaProbeResult probe(const QString &filePath);

private:
    static MediaProbeResult parseJson(const QByteArray &json);
};

} // namespace magnify::engines::ffmpeg
