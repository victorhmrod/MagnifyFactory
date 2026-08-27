#pragma once

#include <QString>
#include <QVariantMap>

#include "core/FormatRegistry.h"

namespace magnify::presets {

// A named shortcut that sets a job's target format and engine parameters in
// one pick — e.g. "YouTube 1080p" means "mp4, crf 20, 1920x1080, 192k audio"
// without the user choosing each setting by hand. Parameters use the same
// keys FFmpegMediaEngine::buildArgsForJob() / PdfEngine already read
// (crf, width, height, audioBitrate, quality, jpegQuality, hardwareBackend,
// ...), so applying a preset is just merging its map into the job's params.
struct Preset {
    QString name;
    magnify::core::FormatCategory category = magnify::core::FormatCategory::Unknown;
    QString targetFormat;
    QVariantMap parameters;

    // User-created presets (via the Preset Manager or a hand-dropped JSON
    // file in presets/) can be deleted from the UI; built-ins cannot.
    // sourceFilePath is empty for built-ins and non-empty (the JSON file
    // backing it) for anything loaded from or saved to presets/.
    bool isUserDefined = false;
    QString sourceFilePath;
};

} // namespace magnify::presets
