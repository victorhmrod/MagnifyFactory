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
    registerFormat({"AVIF", "avif", FormatCategory::Image, {"image/avif"}, true, true});
    registerFormat({"BMP", "bmp", FormatCategory::Image, {"image/bmp"}, true, true});
    registerFormat({"TIFF", "tiff", FormatCategory::Image, {"image/tiff"}, true, true});
    registerFormat({"GIF", "gif", FormatCategory::Image, {"image/gif"}, true, true});

    // --- PDF -----------------------------------------------------------------
    registerFormat({"PDF", "pdf", FormatCategory::Pdf, {"application/pdf"}, true, true});

    // --- Documents (LibreOffice headless) ---------------------------------------
    registerFormat({"Word (docx)", "docx", FormatCategory::Document,
                     {"application/vnd.openxmlformats-officedocument.wordprocessingml.document"}, true, true});
    registerFormat({"Word 97-2003 (doc)", "doc", FormatCategory::Document, {"application/msword"}, true, true});
    registerFormat({"Excel (xlsx)", "xlsx", FormatCategory::Document,
                     {"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"}, true, true});
    registerFormat({"Excel 97-2003 (xls)", "xls", FormatCategory::Document, {"application/vnd.ms-excel"}, true,
                     true});
    registerFormat({"PowerPoint (pptx)", "pptx", FormatCategory::Document,
                     {"application/vnd.openxmlformats-officedocument.presentationml.presentation"}, true, true});
    registerFormat({"PowerPoint 97-2003 (ppt)", "ppt", FormatCategory::Document,
                     {"application/vnd.ms-powerpoint"}, true, true});
    registerFormat({"OpenDocument Text", "odt", FormatCategory::Document, {"application/vnd.oasis.opendocument.text"},
                     true, true});
    registerFormat({"OpenDocument Spreadsheet", "ods", FormatCategory::Document,
                     {"application/vnd.oasis.opendocument.spreadsheet"}, true, true});
    registerFormat({"OpenDocument Presentation", "odp", FormatCategory::Document,
                     {"application/vnd.oasis.opendocument.presentation"}, true, true});
    registerFormat({"Rich Text", "rtf", FormatCategory::Document, {"application/rtf"}, true, true});
    registerFormat({"Plain Text", "txt", FormatCategory::Document, {"text/plain"}, true, true});

    // --- Subtitles (extracted from video via ffmpeg) ---------------------------
    registerFormat({"SubRip", "srt", FormatCategory::Subtitle, {"application/x-subrip"}, true, true});

    // --- Archives --------------------------------------------------------------
    // zip/7z can be created by 7-Zip; rar/tar/gz are extraction-only (creating a
    // proprietary .rar isn't something 7-Zip's free CLI can do).
    registerFormat({"ZIP", "zip", FormatCategory::Archive, {"application/zip"}, true, true});
    registerFormat({"7-Zip", "7z", FormatCategory::Archive, {"application/x-7z-compressed"}, true, true});
    registerFormat({"RAR", "rar", FormatCategory::Archive, {"application/vnd.rar"}, true, false});
    registerFormat({"TAR", "tar", FormatCategory::Archive, {"application/x-tar"}, true, false});
    registerFormat({"GZip", "gz", FormatCategory::Archive, {"application/gzip"}, true, false});
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
