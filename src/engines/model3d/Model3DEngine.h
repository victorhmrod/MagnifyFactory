#pragma once

#include <QHash>
#include <QProcess>

#include "engines/IMediaEngine.h"

namespace magnify::engines::model3d {

// 3D model conversion and basic transforms (scale, rotate, recenter origin,
// decimate/reduce polygon count), backed by Blender's headless CLI
// (`blender --background --factory-startup --python <script> -- --input
// ... --output ...`). The Python script is a bundled resource, written out
// to a shared temp file once and reused, matching the "shell out to a real
// external tool the user installs themselves" policy already used for
// ffmpeg/qpdf/7z/soffice — there is no bundled 3D engine.
class Model3DEngine : public magnify::engines::IMediaEngine {
    Q_OBJECT
public:
    explicit Model3DEngine(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("3D Model Tools"); }

    MediaProbeResult probe(const QString &filePath) override;
    void startConversion(magnify::core::ConversionJob *job) override;
    void cancelConversion(const QUuid &jobId) override;

    // Looks for blender(.exe) on PATH, then common install locations
    // (globbing the versioned "Blender <X.Y>" folder on Windows).
    static QString blenderExecutable();

private:
    void finishJob(magnify::core::ConversionJob *job, bool success, const QString &errorMessage);
    // Writes the bundled conversion script to a shared temp path (once per
    // process) and returns that path, or an empty string on failure.
    QString ensureConversionScript();

    QHash<QUuid, QProcess *> m_runningProcesses;
    QString m_scriptPath;
};

} // namespace magnify::engines::model3d
