#include "Model3DEngine.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include "core/ConversionJob.h"
#include "core/HostProcess.h"

using magnify::core::ConversionJob;
using magnify::core::JobStatus;

namespace magnify::engines::model3d {

namespace {
// Runs headless inside Blender via `blender --background --python <this> --
// <args>`. Import/export operator names changed across Blender versions for
// OBJ/STL/PLY (old import_scene.*/import_mesh.* vs. new wm.*_import/export),
// so each format has a small compatibility shim that tries the modern name
// first. Prints a single "MAGNIFY_RESULT_OK ..." or "MAGNIFY_RESULT_ERROR
// ..." line so the C++ side can tell success from failure without relying on
// exit codes alone (Blender's own exit code is not always reliable across
// versions when an operator raises inside a script).
constexpr const char *kConversionScript = R"PYSCRIPT(
import bpy
import sys
import os
import math
import mathutils

def _args():
    argv = sys.argv
    if "--" not in argv:
        return []
    return argv[argv.index("--") + 1:]

def _parse(argv):
    values = {
        "input": None, "output": None,
        "scale": 1.0, "rotate_x": 0.0, "rotate_y": 0.0, "rotate_z": 0.0,
        "decimate_ratio": 1.0, "center": False,
    }
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg == "--center":
            values["center"] = True
            i += 1
            continue
        key = arg.lstrip("-").replace("-", "_")
        if key in values and i + 1 < len(argv):
            raw = argv[i + 1]
            if key in ("input", "output"):
                values[key] = raw
            else:
                values[key] = float(raw)
            i += 2
        else:
            i += 1
    return values

def import_obj(path):
    if hasattr(bpy.ops.wm, "obj_import"):
        bpy.ops.wm.obj_import(filepath=path)
    else:
        bpy.ops.import_scene.obj(filepath=path)

def export_obj(path):
    if hasattr(bpy.ops.wm, "obj_export"):
        bpy.ops.wm.obj_export(filepath=path)
    else:
        bpy.ops.export_scene.obj(filepath=path)

def import_stl(path):
    if hasattr(bpy.ops.wm, "stl_import"):
        bpy.ops.wm.stl_import(filepath=path)
    else:
        bpy.ops.import_mesh.stl(filepath=path)

def export_stl(path):
    if hasattr(bpy.ops.wm, "stl_export"):
        bpy.ops.wm.stl_export(filepath=path)
    else:
        bpy.ops.export_mesh.stl(filepath=path)

def import_ply(path):
    if hasattr(bpy.ops.wm, "ply_import"):
        bpy.ops.wm.ply_import(filepath=path)
    else:
        bpy.ops.import_mesh.ply(filepath=path)

def export_ply(path):
    if hasattr(bpy.ops.wm, "ply_export"):
        bpy.ops.wm.ply_export(filepath=path)
    else:
        bpy.ops.export_mesh.ply(filepath=path)

def import_model(path):
    ext = os.path.splitext(path)[1].lower()
    if ext == ".obj":
        import_obj(path)
    elif ext == ".fbx":
        bpy.ops.import_scene.fbx(filepath=path)
    elif ext in (".glb", ".gltf"):
        bpy.ops.import_scene.gltf(filepath=path)
    elif ext == ".stl":
        import_stl(path)
    elif ext == ".ply":
        import_ply(path)
    elif ext == ".dae":
        bpy.ops.wm.collada_import(filepath=path)
    else:
        raise ValueError("Unsupported input format: " + ext)

def export_model(path):
    ext = os.path.splitext(path)[1].lower()
    if ext == ".obj":
        export_obj(path)
    elif ext == ".fbx":
        bpy.ops.export_scene.fbx(filepath=path)
    elif ext == ".glb":
        bpy.ops.export_scene.gltf(filepath=path, export_format='GLB')
    elif ext == ".gltf":
        bpy.ops.export_scene.gltf(filepath=path, export_format='GLTF_SEPARATE')
    elif ext == ".stl":
        export_stl(path)
    elif ext == ".ply":
        export_ply(path)
    elif ext == ".dae":
        bpy.ops.wm.collada_export(filepath=path)
    else:
        raise ValueError("Unsupported output format: " + ext)

def main():
    args = _parse(_args())
    if not args["input"] or not args["output"]:
        print("MAGNIFY_RESULT_ERROR --input and --output are required")
        sys.exit(1)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    import_model(args["input"])

    mesh_objects = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    if not mesh_objects:
        print("MAGNIFY_RESULT_ERROR No mesh data found after import")
        sys.exit(1)

    bpy.ops.object.select_all(action='DESELECT')
    for obj in mesh_objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = mesh_objects[0]

    if args["scale"] != 1.0:
        for obj in mesh_objects:
            obj.scale = obj.scale * args["scale"]

    if args["rotate_x"] or args["rotate_y"] or args["rotate_z"]:
        for obj in mesh_objects:
            obj.rotation_euler.x += math.radians(args["rotate_x"])
            obj.rotation_euler.y += math.radians(args["rotate_y"])
            obj.rotation_euler.z += math.radians(args["rotate_z"])

    for obj in mesh_objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = mesh_objects[0]
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)

    if args["center"]:
        min_co = mathutils.Vector((float("inf"),) * 3)
        max_co = mathutils.Vector((float("-inf"),) * 3)
        for obj in mesh_objects:
            for corner in obj.bound_box:
                world_co = obj.matrix_world @ mathutils.Vector(corner)
                min_co = mathutils.Vector(min(a, b) for a, b in zip(min_co, world_co))
                max_co = mathutils.Vector(max(a, b) for a, b in zip(max_co, world_co))
        offset = (min_co + max_co) / 2
        for obj in mesh_objects:
            obj.location = obj.location - offset

    if args["decimate_ratio"] < 1.0:
        for obj in mesh_objects:
            mod = obj.modifiers.new(name="MagnifyDecimate", type='DECIMATE')
            mod.ratio = max(0.01, min(1.0, args["decimate_ratio"]))
            bpy.context.view_layer.objects.active = obj
            bpy.ops.object.modifier_apply(modifier=mod.name)

    export_model(args["output"])

    total_verts = sum(len(o.data.vertices) for o in mesh_objects)
    total_faces = sum(len(o.data.polygons) for o in mesh_objects)
    print("MAGNIFY_RESULT_OK verts=%d faces=%d" % (total_verts, total_faces))

try:
    main()
except Exception as exc:
    print("MAGNIFY_RESULT_ERROR " + str(exc))
    sys.exit(1)
)PYSCRIPT";
} // namespace

Model3DEngine::Model3DEngine(QObject *parent) : IMediaEngine(parent) {
}

QString Model3DEngine::blenderExecutable() {
    // Same reasoning as DocumentEngine::sofficeExecutable(): inside a
    // Flatpak sandbox, PATH lookups only see the sandbox, so hand the bare
    // command to flatpak-spawn --host and let it resolve against the host.
    if (magnify::core::HostProcess::isSandboxed()) {
        return QStringLiteral("blender");
    }

    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("blender"));
    if (!onPath.isEmpty()) {
        return onPath;
    }

#if defined(Q_OS_WIN)
    const QDir foundationDir(QStringLiteral("C:/Program Files/Blender Foundation"));
    if (foundationDir.exists()) {
        // Newest version first: entryList with Reversed sort on names like
        // "Blender 5.1" sorts lexicographically, which is close enough to
        // version order for the single-digit major/minor versions Blender
        // has used to date.
        const QStringList versionDirs = foundationDir.entryList(
            {QStringLiteral("Blender*")}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
        for (const QString &versionDir : versionDirs) {
            const QString candidate = foundationDir.filePath(versionDir + QStringLiteral("/blender.exe"));
            if (QFileInfo::exists(candidate)) {
                return candidate;
            }
        }
    }
#elif defined(Q_OS_MACOS)
    const QStringList candidates{QStringLiteral("/Applications/Blender.app/Contents/MacOS/Blender")};
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
#else
    const QStringList candidates{
        QStringLiteral("/usr/bin/blender"),
        QStringLiteral("/snap/bin/blender"),
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
#endif
    return QString();
}

QString Model3DEngine::ensureConversionScript() {
    if (!m_scriptPath.isEmpty() && QFileInfo::exists(m_scriptPath)) {
        return m_scriptPath;
    }

    const QString path = QDir(magnify::core::HostProcess::sharedTempDir()).filePath(QStringLiteral("magnify_model3d_convert.py"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return QString();
    }
    file.write(kConversionScript);
    file.close();
    m_scriptPath = path;
    return m_scriptPath;
}

MediaProbeResult Model3DEngine::probe(const QString &filePath) {
    MediaProbeResult result;
    result.valid = QFileInfo(filePath).exists();
    if (!result.valid) {
        result.errorMessage = QStringLiteral("File does not exist: %1").arg(filePath);
    }
    return result;
}

void Model3DEngine::startConversion(ConversionJob *job) {
    job->setStatus(JobStatus::Preparing);

    const QString blender = blenderExecutable();
    if (blender.isEmpty()) {
        finishJob(job, false, QStringLiteral("Blender was not found. Install it to convert/edit 3D models."));
        return;
    }

    const QString scriptPath = ensureConversionScript();
    if (scriptPath.isEmpty()) {
        finishJob(job, false, QStringLiteral("Could not write the Blender conversion script to a temp file."));
        return;
    }

    QDir().mkpath(QFileInfo(job->outputPath()).absolutePath());

    const auto &params = job->parameters();
    QStringList scriptArgs{
        QStringLiteral("--input"), job->inputPath(),
        QStringLiteral("--output"), job->outputPath(),
        QStringLiteral("--scale"), QString::number(params.value(QStringLiteral("scale"), 1.0).toDouble()),
        QStringLiteral("--rotate-x"), QString::number(params.value(QStringLiteral("rotateX"), 0.0).toDouble()),
        QStringLiteral("--rotate-y"), QString::number(params.value(QStringLiteral("rotateY"), 0.0).toDouble()),
        QStringLiteral("--rotate-z"), QString::number(params.value(QStringLiteral("rotateZ"), 0.0).toDouble()),
        QStringLiteral("--decimate-ratio"),
        QString::number(params.value(QStringLiteral("decimateRatio"), 1.0).toDouble()),
    };
    if (params.value(QStringLiteral("center"), false).toBool()) {
        scriptArgs << QStringLiteral("--center");
    }

    QStringList args{
        QStringLiteral("--background"),
        QStringLiteral("--factory-startup"),
        QStringLiteral("--python"), scriptPath,
        QStringLiteral("--"),
    };
    args << scriptArgs;

    job->setStatus(JobStatus::Running);

    auto *process = new QProcess(this);
    m_runningProcesses.insert(job->id(), process);

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, job, process](int exitCode, QProcess::ExitStatus exitStatus) {
                m_runningProcesses.remove(job->id());

                const QByteArray stdOut = process->readAllStandardOutput();
                const bool reportedOk = stdOut.contains("MAGNIFY_RESULT_OK");
                const bool success = exitStatus == QProcess::NormalExit && exitCode == 0 && reportedOk &&
                                      QFileInfo::exists(job->outputPath());

                QString error;
                if (!success) {
                    const QByteArray stdErr = process->readAllStandardError();
                    const int errorMarker = stdOut.indexOf("MAGNIFY_RESULT_ERROR");
                    if (errorMarker >= 0) {
                        error = QString::fromUtf8(stdOut.mid(errorMarker));
                    } else {
                        error = QStringLiteral("Blender conversion failed: %1")
                                    .arg(QString::fromUtf8(stdErr.isEmpty() ? stdOut : stdErr));
                    }
                }
                finishJob(job, success, error);
                process->deleteLater();
            });

    connect(process, &QProcess::errorOccurred, this, [this, job](QProcess::ProcessError) {
        if (m_runningProcesses.contains(job->id())) {
            finishJob(job, false, QStringLiteral("Failed to start Blender."));
        }
    });

    magnify::core::HostProcess::start(process, blender, args);
}

void Model3DEngine::finishJob(ConversionJob *job, bool success, const QString &errorMessage) {
    if (success) {
        job->setProgressPercent(100);
        job->setStatus(JobStatus::Completed);
    } else {
        job->setErrorMessage(errorMessage);
        job->setStatus(JobStatus::Failed);
    }
    emit jobFinished(job->id(), success, errorMessage);
}

void Model3DEngine::cancelConversion(const QUuid &jobId) {
    if (auto *process = m_runningProcesses.value(jobId)) {
        process->kill();
    }
}

} // namespace magnify::engines::model3d
