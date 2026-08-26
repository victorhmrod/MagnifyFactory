#include "FormatRegistry.h"

namespace magnify::core {

FormatRegistry &FormatRegistry::instance() {
    static FormatRegistry registry;
    return registry;
}

FormatRegistry::FormatRegistry() {
    registerBuiltins();
}

void FormatRegistry::registerFormat(const FormatDescriptor &descriptor) {
    m_formats.push_back(descriptor);
}

void FormatRegistry::registerBuiltins() {
    // --- Video containers -------------------------------------------------
    registerFormat({"MP4", "mp4", FormatCategory::Video, {"video/mp4"}, true, true});
    registerFormat({"Matroska", "mkv", FormatCategory::Video, {"video/x-matroska"}, true, true});
    registerFormat({"QuickTime", "mov", FormatCategory::Video, {"video/quicktime"}, true, true});
    registerFormat({"AVI", "avi", FormatCategory::Video, {"video/x-msvideo"}, true, true});
    registerFormat({"MPEG", "mpeg", FormatCategory::Video, {"video/mpeg"}, true, true});
    registerFormat({"MPEG", "mpg", FormatCategory::Video, {"video/mpeg"}, true, true});
    registerFormat({"WebM", "webm", FormatCategory::Video, {"video/webm"}, true, true});
    registerFormat({"FLV", "flv", FormatCategory::Video, {"video/x-flv"}, true, true});

    // --- Audio containers ---------------------------------------------------
    registerFormat({"MP3", "mp3", FormatCategory::Audio, {"audio/mpeg"}, true, true});
    registerFormat({"WAV", "wav", FormatCategory::Audio, {"audio/wav"}, true, true});
    registerFormat({"FLAC", "flac", FormatCategory::Audio, {"audio/flac"}, true, true});
    registerFormat({"AAC", "aac", FormatCategory::Audio, {"audio/aac"}, true, true});
    registerFormat({"M4A", "m4a", FormatCategory::Audio, {"audio/mp4"}, true, true});
    registerFormat({"OGG", "ogg", FormatCategory::Audio, {"audio/ogg"}, true, true});

    // --- Images (registered now, engine arrives in Fase 2) -----------------
    registerFormat({"PNG", "png", FormatCategory::Image, {"image/png"}, true, true});
    registerFormat({"JPEG", "jpg", FormatCategory::Image, {"image/jpeg"}, true, true});
    registerFormat({"JPEG", "jpeg", FormatCategory::Image, {"image/jpeg"}, true, true});
    registerFormat({"WebP", "webp", FormatCategory::Image, {"image/webp"}, true, true});

    // --- PDF -----------------------------------------------------------------
    registerFormat({"PDF", "pdf", FormatCategory::Pdf, {"application/pdf"}, true, true});
}

const FormatDescriptor *FormatRegistry::findByExtension(const QString &extension) const {
    const QString normalized = extension.toLower();
    for (const auto &descriptor : m_formats) {
        if (descriptor.extension == normalized) {
            return &descriptor;
        }
    }
    return nullptr;
}

FormatCategory FormatRegistry::categoryOf(const QString &extension) const {
    if (const auto *descriptor = findByExtension(extension)) {
        return descriptor->category;
    }
    return FormatCategory::Unknown;
}

QVector<FormatDescriptor> FormatRegistry::formatsInCategory(FormatCategory category) const {
    QVector<FormatDescriptor> result;
    for (const auto &descriptor : m_formats) {
        if (descriptor.category == category) {
            result.push_back(descriptor);
        }
    }
    return result;
}

} // namespace magnify::core
