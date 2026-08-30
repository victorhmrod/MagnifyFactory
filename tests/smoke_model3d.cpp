// Standalone smoke test: drives the real Model3DEngine (Blender headless)
// through an OBJ -> GLB conversion with a scale transform, and verifies the
// output for real by parsing the GLB container directly (no bpy involved on
// the read side, so this is a genuinely independent check of what Blender's
// exporter wrote) — asserts vertex count and that the geometry was actually
// scaled. Not part of ctest — shells out to a real Blender install and
// needs Python 3 on PATH for the independent verification step; run
// manually during 3D model tooling verification.
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryFile>
#include <QTimer>
#include <QDebug>

#include "core/ConversionJob.h"
#include "core/JobManager.h"
#include "engines/model3d/Model3DEngine.h"

using magnify::core::ConversionJob;
using magnify::core::JobManager;
using magnify::core::JobStatus;

namespace {
// Parses a .glb file's JSON chunk directly (magic + chunk framing per the
// glTF 2.0 binary spec) using plain Python, independent of Blender/bpy, and
// prints "TRIS <n>" (triangle count, from the indices accessor — stable
// regardless of exporter vertex-splitting at hard edges/UV seams, unlike a
// raw vertex count) and "BOUNDS <minX> <maxX>" (from the POSITION
// accessor's declared min/max, which glTF exporters are required to
// populate).
const char *kGlbVerifyScript = R"PY(
import json
import struct
import sys

path = sys.argv[1]
with open(path, "rb") as f:
    data = f.read()

magic, version, length = struct.unpack_from("<4sII", data, 0)
if magic != b"glTF":
    print("FAIL not a glb file")
    sys.exit(1)

offset = 12
json_chunk = None
while offset < length:
    chunk_length, chunk_type = struct.unpack_from("<I4s", data, offset)
    chunk_data = data[offset + 8: offset + 8 + chunk_length]
    if chunk_type == b"JSON":
        json_chunk = json.loads(chunk_data.decode("utf-8"))
        break
    offset += 8 + chunk_length

if json_chunk is None:
    print("FAIL no JSON chunk found")
    sys.exit(1)

meshes = json_chunk.get("meshes", [])
if not meshes:
    print("FAIL no meshes in glb")
    sys.exit(1)

primitive = meshes[0]["primitives"][0]
indices_accessor = json_chunk["accessors"][primitive["indices"]]
print("TRIS %d" % (indices_accessor["count"] // 3))

pos_accessor = json_chunk["accessors"][primitive["attributes"]["POSITION"]]
mn, mx = pos_accessor["min"], pos_accessor["max"]
print("BOUNDS %f %f" % (mn[0], mx[0]))
)PY";

bool runJob(JobManager &manager, std::unique_ptr<ConversionJob> job) {
    ConversionJob *raw = job.get();
    QEventLoop loop;
    bool finished = false, ok = false;
    QObject::connect(raw, &ConversionJob::statusChanged, &loop, [&](JobStatus status) {
        if (status == JobStatus::Completed || status == JobStatus::Failed || status == JobStatus::Cancelled) {
            finished = true;
            ok = (status == JobStatus::Completed);
            loop.quit();
        }
    });
    QTimer::singleShot(120000, &loop, &QEventLoop::quit); // Blender's startup is slow.
    manager.addJob(std::move(job));
    manager.startQueue();
    if (!finished) {
        loop.exec();
    }
    if (!finished) {
        fprintf(stderr, "TIMEOUT\n");
        return false;
    }
    if (!ok) {
        fprintf(stderr, "FAILED: %s\n", qPrintable(raw->errorMessage()));
        return false;
    }
    return true;
}
} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    const QString dir = QDir::currentPath();
    bool allOk = true;

    // A unit cube (vertices at +/-1 on each axis), plain-text OBJ.
    const QString cubeObj = dir + "/model3d_cube.obj";
    {
        QFile f(cubeObj);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            fprintf(stderr, "FAILED: could not write cube.obj\n");
            return 1;
        }
        f.write("v -1 -1 -1\nv 1 -1 -1\nv 1 1 -1\nv -1 1 -1\n"
                "v -1 -1 1\nv 1 -1 1\nv 1 1 1\nv -1 1 1\n"
                "f 1 2 3 4\nf 5 8 7 6\nf 1 5 6 2\nf 2 6 7 3\nf 3 7 8 4\nf 5 1 4 8\n");
    }

    magnify::engines::model3d::Model3DEngine engine;
    if (magnify::engines::model3d::Model3DEngine::blenderExecutable().isEmpty()) {
        fprintf(stderr, "SKIPPED: Blender not found on this machine\n");
        return 0;
    }

    JobManager manager;
    manager.registerEngine(&engine);

    // Scale 2x, rotate 45 degrees around Z, recenter the origin — output as
    // GLB, which is easy to independently re-parse without bpy.
    const QString outGlb = dir + "/model3d_edited.glb";
    auto job = std::make_unique<ConversionJob>(cubeObj, outGlb);
    job->setSourceFormat("obj");
    job->setTargetFormat("glb");
    job->setEngineName("3D Model Tools");
    job->setParameters({
        {"scale", 2.0},
        {"rotateZ", 45.0},
        {"center", true},
    });

    if (!runJob(manager, std::move(job))) {
        fprintf(stderr, "FAILED: model3d conversion did not complete\n");
        return 1;
    }

    if (!QFileInfo::exists(outGlb) || QFileInfo(outGlb).size() == 0) {
        fprintf(stderr, "FAILED: output glb missing or empty\n");
        return 1;
    }
    fprintf(stderr, "OK: produced %s (%lld bytes)\n", qPrintable(outGlb), (long long)QFileInfo(outGlb).size());

    // Independent verification: parse the glb container directly (no bpy).
    QTemporaryFile scriptFile(dir + "/glb_verify_XXXXXX.py");
    scriptFile.setAutoRemove(false);
    if (!scriptFile.open()) {
        fprintf(stderr, "FAILED: could not write verify script\n");
        return 1;
    }
    scriptFile.write(kGlbVerifyScript);
    const QString scriptPath = scriptFile.fileName();
    scriptFile.close();

    QProcess verify;
    verify.start("python", {scriptPath, outGlb});
    if (!verify.waitForStarted(5000)) {
        verify.start("python3", {scriptPath, outGlb});
    }
    verify.waitForFinished(15000);
    const QString verifyOut = QString::fromUtf8(verify.readAllStandardOutput());
    QFile::remove(scriptPath);

    // 6 quad faces -> 12 triangles once glTF triangulates on export.
    if (!verifyOut.contains("TRIS 12")) {
        fprintf(stderr, "FAILED: expected 12 triangles (6 quad faces, triangulated), got:\n%s\n",
                qPrintable(verifyOut));
        allOk = false;
    } else {
        fprintf(stderr, "OK: glb has 12 triangles, matching the source cube's 6 quad faces\n");
    }

    // Original cube spans [-1, 1] on each axis (extent 2); after a 2x scale
    // (and unaffected-by-rotation extent on a symmetric cube), the X extent
    // should be roughly doubled to ~4 (rotation/centering can shift the
    // absolute min/max, so check the SPAN, not absolute position).
    const QStringList lines = verifyOut.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (line.startsWith("BOUNDS ")) {
            const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            if (parts.size() == 3) {
                const double span = parts.at(2).toDouble() - parts.at(1).toDouble();
                // A 45-degree rotation of a scaled cube changes its
                // axis-aligned bounding span (diagonal effect), so just
                // check it grew well beyond the original span of 2 — a
                // scale bug that silently no-op'd would leave it at ~2.
                if (span < 3.0) {
                    fprintf(stderr, "FAILED: X span %.3f looks unscaled (expected > 3 after a 2x scale)\n", span);
                    allOk = false;
                } else {
                    fprintf(stderr, "OK: X span is %.3f (source was 2, so the 2x scale took effect)\n", span);
                }
            }
        }
    }

    qInfo() << (allOk ? "ALL 3D MODEL EDITOR CHECKS PASSED" : "SOME 3D MODEL EDITOR CHECKS FAILED");
    return allOk ? 0 : 1;
}
