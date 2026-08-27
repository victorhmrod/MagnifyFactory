#include "PresetRegistry.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using magnify::core::FormatCategory;

namespace magnify::presets {

namespace {
Preset makePreset(const QString &name, FormatCategory category, const QString &targetFormat,
                   const QVariantMap &parameters) {
    return Preset{name, category, targetFormat, parameters, false, QString()};
}

QString categoryToString(FormatCategory category) {
    switch (category) {
        case FormatCategory::Video: return QStringLiteral("video");
        case FormatCategory::Audio: return QStringLiteral("audio");
        case FormatCategory::Image: return QStringLiteral("image");
        case FormatCategory::Pdf: return QStringLiteral("pdf");
        default: return QString();
    }
}

// Turns a preset name into a filesystem-safe filename stem — strips
// anything but letters/digits/space/dash/underscore so a name like
// "My Preset (v2)" doesn't produce an invalid or surprising path.
QString sanitizeFileName(const QString &name) {
    QString result;
    for (const QChar &ch : name) {
        if (ch.isLetterOrNumber() || ch == QChar(' ') || ch == QChar('-') || ch == QChar('_')) {
            result += ch;
        }
    }
    result = result.trimmed();
    return result.isEmpty() ? QStringLiteral("preset") : result;
}
} // namespace

PresetRegistry &PresetRegistry::instance() {
    static PresetRegistry registry;
    return registry;
}

PresetRegistry::PresetRegistry() {
    registerBuiltins();
    loadUserPresets();
}

void PresetRegistry::registerBuiltins() {
    using C = FormatCategory;

    m_presets << makePreset(QStringLiteral("YouTube 1080p"), C::Video, QStringLiteral("mp4"),
                             {{"crf", 20}, {"width", 1920}, {"height", 1080}, {"audioBitrate", "192k"}});
    m_presets << makePreset(QStringLiteral("YouTube 4K"), C::Video, QStringLiteral("mp4"),
                             {{"crf", 18}, {"width", 3840}, {"height", 2160}, {"audioBitrate", "192k"}});
    m_presets << makePreset(QStringLiteral("Discord"), C::Video, QStringLiteral("mp4"),
                             {{"crf", 28}, {"width", 1280}, {"height", 720}, {"audioBitrate", "128k"}});
    m_presets << makePreset(QStringLiteral("WhatsApp"), C::Video, QStringLiteral("mp4"),
                             {{"crf", 30}, {"width", 854}, {"height", 480}, {"audioBitrate", "96k"}});

    m_presets << makePreset(QStringLiteral("MP3 320 kbps"), C::Audio, QStringLiteral("mp3"),
                             {{"audioBitrate", "320k"}});
    m_presets << makePreset(QStringLiteral("FLAC (Lossless)"), C::Audio, QStringLiteral("flac"), {});

    m_presets << makePreset(QStringLiteral("WebP (Web)"), C::Image, QStringLiteral("webp"), {{"quality", 80}});
    m_presets << makePreset(QStringLiteral("AVIF (Web)"), C::Image, QStringLiteral("avif"), {{"crf", 32}});

    m_presets << makePreset(QStringLiteral("Small Size"), C::Pdf, QStringLiteral("pdf"), {{"jpegQuality", 40}});
    m_presets << makePreset(QStringLiteral("High Quality"), C::Pdf, QStringLiteral("pdf"), {{"jpegQuality", 90}});
    m_presets << makePreset(QStringLiteral("Split (one file per page)"), C::Pdf, QStringLiteral("pdf"),
                             {{"operation", "split"}});
}

void PresetRegistry::loadUserPresets() {
    const QDir presetsDir(QCoreApplication::applicationDirPath() + QStringLiteral("/presets"));
    if (!presetsDir.exists()) {
        return;
    }

    for (const QFileInfo &fileInfo : presetsDir.entryInfoList({QStringLiteral("*.json")}, QDir::Files)) {
        QFile file(fileInfo.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (!doc.isObject()) {
            continue;
        }
        const QJsonObject obj = doc.object();
        const QString name = obj.value(QStringLiteral("name")).toString();
        const QString categoryStr = obj.value(QStringLiteral("category")).toString().toLower();
        const QString targetFormat = obj.value(QStringLiteral("targetFormat")).toString();
        if (name.isEmpty() || targetFormat.isEmpty()) {
            continue;
        }

        FormatCategory category = FormatCategory::Unknown;
        if (categoryStr == QStringLiteral("video")) category = FormatCategory::Video;
        else if (categoryStr == QStringLiteral("audio")) category = FormatCategory::Audio;
        else if (categoryStr == QStringLiteral("image")) category = FormatCategory::Image;
        else if (categoryStr == QStringLiteral("pdf")) category = FormatCategory::Pdf;
        if (category == FormatCategory::Unknown) {
            continue;
        }

        QVariantMap parameters;
        const QJsonObject paramsObj = obj.value(QStringLiteral("parameters")).toObject();
        for (auto it = paramsObj.constBegin(); it != paramsObj.constEnd(); ++it) {
            parameters.insert(it.key(), it.value().toVariant());
        }

        Preset preset = makePreset(name, category, targetFormat, parameters);
        preset.isUserDefined = true;
        preset.sourceFilePath = fileInfo.absoluteFilePath();
        m_presets << preset;
    }
}

void PresetRegistry::addUserPreset(Preset preset) {
    QDir presetsDir(QCoreApplication::applicationDirPath() + QStringLiteral("/presets"));
    if (!presetsDir.exists()) {
        presetsDir.mkpath(QStringLiteral("."));
    }

    QString filePath = presetsDir.filePath(sanitizeFileName(preset.name) + QStringLiteral(".json"));
    int suffix = 2;
    while (QFileInfo::exists(filePath)) {
        filePath = presetsDir.filePath(
            QStringLiteral("%1 %2.json").arg(sanitizeFileName(preset.name)).arg(suffix++));
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("name"), preset.name);
    obj.insert(QStringLiteral("category"), categoryToString(preset.category));
    obj.insert(QStringLiteral("targetFormat"), preset.targetFormat);
    QJsonObject paramsObj;
    for (auto it = preset.parameters.constBegin(); it != preset.parameters.constEnd(); ++it) {
        paramsObj.insert(it.key(), QJsonValue::fromVariant(it.value()));
    }
    obj.insert(QStringLiteral("parameters"), paramsObj);

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
        file.close();
    }

    preset.isUserDefined = true;
    preset.sourceFilePath = filePath;
    m_presets << preset;
}

bool PresetRegistry::removeUserPreset(const QString &name) {
    for (int i = 0; i < m_presets.size(); ++i) {
        if (m_presets[i].isUserDefined && m_presets[i].name == name) {
            if (!m_presets[i].sourceFilePath.isEmpty()) {
                QFile::remove(m_presets[i].sourceFilePath);
            }
            m_presets.remove(i);
            return true;
        }
    }
    return false;
}

QVector<Preset> PresetRegistry::presetsForCategory(FormatCategory category) const {
    QVector<Preset> result;
    for (const Preset &preset : m_presets) {
        if (preset.category == category) {
            result.append(preset);
        }
    }
    return result;
}

} // namespace magnify::presets
