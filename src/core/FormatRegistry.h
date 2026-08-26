#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace magnify::core {

enum class FormatCategory {
    Video,
    Audio,
    Image,
    Pdf,
    Unknown
};

struct FormatDescriptor {
    QString name;          // Human readable name, e.g. "MP4"
    QString extension;     // Lowercase extension without dot, e.g. "mp4"
    FormatCategory category = FormatCategory::Unknown;
    QStringList mimeTypes;
    bool supportsInput = true;
    bool supportsOutput = true;
};

// Central, non-hardcoded registry of every format MagnifyFactory understands.
// Engines and UI must query this instead of scattering extension literals.
class FormatRegistry {
public:
    static FormatRegistry &instance();

    const FormatDescriptor *findByExtension(const QString &extension) const;
    FormatCategory categoryOf(const QString &extension) const;
    QVector<FormatDescriptor> formatsInCategory(FormatCategory category) const;
    const QVector<FormatDescriptor> &all() const { return m_formats; }

    void registerFormat(const FormatDescriptor &descriptor);

private:
    FormatRegistry();
    void registerBuiltins();

    QVector<FormatDescriptor> m_formats;
};

} // namespace magnify::core
