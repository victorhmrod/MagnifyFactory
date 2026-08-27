// Standalone smoke test: exercises the user preset CRUD (PresetRegistry::
// addUserPreset/removeUserPreset) end to end — the JSON file actually lands
// on disk with the right content, the preset is usable for a real
// conversion (verified via ffprobe, including a genuine upscale), and
// removing it deletes the file and drops it from the registry. Not part of
// ctest — shells out to real ffmpeg/ffprobe and touches the presets/
// folder next to the built executable.
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTimer>

#include "core/ConversionJob.h"
#include "core/JobManager.h"
#include "engines/ffmpeg/FFmpegMediaEngine.h"
#include "presets/PresetRegistry.h"

using magnify::core::ConversionJob;
using magnify::core::FormatCategory;
using magnify::core::JobManager;
using magnify::core::JobStatus;
using magnify::presets::Preset;
using magnify::presets::PresetRegistry;

namespace {
bool findPreset(const QString &name, Preset *out) {
    for (const Preset &p : PresetRegistry::instance().all()) {
        if (p.name == name) {
            *out = p;
            return true;
        }
    }
    return false;
}
} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    const QString dir = QDir::currentPath();
    bool allOk = true;

    const QString presetName = QStringLiteral("Smoke Test Upscale");

    // --- Create: register + persist, verify the JSON file on disk --------
    Preset newPreset{presetName,
                      FormatCategory::Video,
                      QStringLiteral("mp4"),
                      {{"crf", 20}, {"width", 1280}, {"height", 720}},
                      false,
                      QString()};
    PresetRegistry::instance().addUserPreset(newPreset);

    Preset stored;
    if (!findPreset(presetName, &stored) || !stored.isUserDefined || stored.sourceFilePath.isEmpty()) {
        fprintf(stderr, "FAILED: created preset not found (or not marked user-defined) in registry\n");
        return 1;
    }
    if (!QFileInfo::exists(stored.sourceFilePath)) {
        fprintf(stderr, "FAILED: preset JSON file was not written to %s\n", qPrintable(stored.sourceFilePath));
        return 1;
    }
    QFile jsonFile(stored.sourceFilePath);
    jsonFile.open(QIODevice::ReadOnly);
    const QJsonObject obj = QJsonDocument::fromJson(jsonFile.readAll()).object();
    jsonFile.close();
    if (obj.value("name").toString() != presetName || obj.value("targetFormat").toString() != "mp4" ||
        obj.value("parameters").toObject().value("width").toInt() != 1280) {
        fprintf(stderr, "FAILED: preset JSON content doesn't match what was created\n");
        return 1;
    }
    fprintf(stderr, "OK: preset created and persisted at %s\n", qPrintable(stored.sourceFilePath));

    // --- Use it for a real conversion: confirm the upscale actually happens
    const QString sourcePath = dir + "/custom_preset_src.mp4";
    QProcess gen;
    gen.start("ffmpeg", {"-y", "-f", "lavfi", "-i", "testsrc=duration=1:size=320x240:rate=10", "-c:v", "libx264",
                          sourcePath});
    gen.waitForFinished(15000);
    if (!QFileInfo::exists(sourcePath)) {
        fprintf(stderr, "FAILED: could not generate synthetic source video\n");
        return 1;
    }

    magnify::engines::ffmpeg::FFmpegMediaEngine engine;
    JobManager manager;
    manager.registerEngine(&engine);

    const QString outPath = dir + "/custom_preset_out.mp4";
    auto job = std::make_unique<ConversionJob>(sourcePath, outPath);
    job->setSourceFormat("mp4");
    job->setTargetFormat(stored.targetFormat);
    job->setEngineName("FFmpeg");
    job->setParameters(stored.parameters);
    ConversionJob *raw = manager.addJob(std::move(job));

    QEventLoop loop;
    bool finished = false, ok = false;
    QObject::connect(raw, &ConversionJob::statusChanged, &loop, [&](JobStatus status) {
        if (status == JobStatus::Completed || status == JobStatus::Failed || status == JobStatus::Cancelled) {
            finished = true;
            ok = (status == JobStatus::Completed);
            loop.quit();
        }
    });
    QTimer::singleShot(30000, &loop, &QEventLoop::quit);
    manager.startQueue();
    loop.exec();

    if (!finished || !ok) {
        fprintf(stderr, "FAILED: conversion with custom preset did not complete: %s\n",
                qPrintable(raw->errorMessage()));
        allOk = false;
    } else {
        QProcess probe;
        probe.start("ffprobe", {"-v", "error", "-select_streams", "v:0", "-show_entries", "stream=width,height",
                                 "-of", "csv=p=0", outPath});
        probe.waitForFinished(10000);
        const QString dims = QString::fromUtf8(probe.readAllStandardOutput()).trimmed();
        if (dims != "1280,720") {
            fprintf(stderr, "FAILED: expected upscale to 1280x720, got %s\n", qPrintable(dims));
            allOk = false;
        } else {
            fprintf(stderr, "OK: 320x240 source upscaled to %s via the custom preset\n", qPrintable(dims));
        }
    }

    // --- Delete: JSON file removed, preset gone from the registry --------
    const QString removedPath = stored.sourceFilePath;
    if (!PresetRegistry::instance().removeUserPreset(presetName)) {
        fprintf(stderr, "FAILED: removeUserPreset reported no match\n");
        allOk = false;
    } else if (QFileInfo::exists(removedPath)) {
        fprintf(stderr, "FAILED: preset JSON file still exists after removal\n");
        allOk = false;
    } else {
        Preset shouldBeGone;
        if (findPreset(presetName, &shouldBeGone)) {
            fprintf(stderr, "FAILED: preset still present in registry after removal\n");
            allOk = false;
        } else {
            fprintf(stderr, "OK: preset file deleted and removed from the registry\n");
        }
    }

    fprintf(stderr, "%s\n", allOk ? "ALL CUSTOM PRESET CHECKS PASSED" : "SOME CUSTOM PRESET CHECKS FAILED");
    return allOk ? 0 : 1;
}
