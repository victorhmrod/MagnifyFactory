// Standalone smoke test: loads the real sample plugin DLL
// (build/plugins/magnify_plugin_sample.dll) via PluginManager and confirms
// its contributed preset actually lands in PresetRegistry — proving the
// plugin API works across a real DLL boundary, not just in source. Not part
// of ctest (depends on a sibling build target); run manually.
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QProcess>
#include <QTimer>
#include <cstdio>

#include "core/ConversionJob.h"
#include "core/FormatRegistry.h"
#include "core/JobManager.h"
#include "engines/ffmpeg/FFmpegMediaEngine.h"
#include "plugins/PluginManager.h"
#include "presets/PresetRegistry.h"

using magnify::core::ConversionJob;
using magnify::core::FormatCategory;
using magnify::core::JobManager;
using magnify::core::JobStatus;
using magnify::plugins::PluginManager;
using magnify::presets::Preset;
using magnify::presets::PresetRegistry;

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    const QString pluginsDir = QDir::currentPath();

    PluginManager manager;
    manager.loadPluginsFrom(pluginsDir);

    if (manager.loadedPlugins().isEmpty()) {
        fprintf(stderr, "FAILED: no plugins loaded from %s\n", qPrintable(pluginsDir));
        return 1;
    }

    bool foundExpectedPlugin = false;
    for (const auto &loaded : manager.loadedPlugins()) {
        fprintf(stderr, "Loaded plugin: %s v%s (%s)\n", qPrintable(loaded.plugin->name()),
                qPrintable(loaded.plugin->version()), qPrintable(loaded.filePath));
        if (loaded.plugin->name() == QStringLiteral("Sample WhatsApp Status Preset")) {
            foundExpectedPlugin = true;
        }
    }
    if (!foundExpectedPlugin) {
        fprintf(stderr, "FAILED: expected sample plugin was not among the loaded plugins\n");
        return 1;
    }

    Preset contributedPreset;
    bool foundContributedPreset = false;
    for (const Preset &preset : PresetRegistry::instance().presetsForCategory(FormatCategory::Video)) {
        if (preset.name == QStringLiteral("Plugin: WhatsApp Status")) {
            contributedPreset = preset;
            foundContributedPreset = true;
        }
    }
    if (!foundContributedPreset) {
        fprintf(stderr, "FAILED: plugin loaded but its preset never reached PresetRegistry\n");
        return 1;
    }

    // Don't just check the preset's fields — actually run a real conversion
    // with it, through the same JobManager + FFmpegMediaEngine the app uses,
    // and confirm the output really is the resolution the plugin asked for.
    const QString dir = QDir::currentPath();
    const QString sourcePath = dir + "/plugin_smoke_src.mp4";
    QProcess gen;
    gen.start("ffmpeg", {"-y", "-f", "lavfi", "-i", "testsrc=duration=1:size=640x360:rate=10", "-c:v", "libx264",
                          sourcePath});
    gen.waitForFinished(15000);
    if (!QFileInfo::exists(sourcePath)) {
        fprintf(stderr, "FAILED: could not generate synthetic source video\n");
        return 1;
    }

    magnify::engines::ffmpeg::FFmpegMediaEngine engine;
    JobManager jobManager;
    jobManager.registerEngine(&engine);

    const QString outPath = dir + "/plugin_smoke_out." + contributedPreset.targetFormat;
    auto job = std::make_unique<ConversionJob>(sourcePath, outPath);
    job->setSourceFormat("mp4");
    job->setTargetFormat(contributedPreset.targetFormat);
    job->setEngineName("FFmpeg");
    job->setParameters(contributedPreset.parameters);
    ConversionJob *raw = jobManager.addJob(std::move(job));

    QEventLoop loop;
    bool finished = false, ok = false;
    QObject::connect(raw, &ConversionJob::statusChanged, &loop, [&](JobStatus status) {
        if (status == JobStatus::Completed || status == JobStatus::Failed) {
            finished = true;
            ok = (status == JobStatus::Completed);
            loop.quit();
        }
    });
    QTimer::singleShot(30000, &loop, &QEventLoop::quit);
    jobManager.startQueue();
    loop.exec();

    if (!finished || !ok) {
        fprintf(stderr, "FAILED: conversion using the plugin's preset did not complete: %s\n",
                qPrintable(raw->errorMessage()));
        return 1;
    }

    QProcess probe;
    probe.start("ffprobe", {"-v", "error", "-select_streams", "v:0", "-show_entries", "stream=width,height", "-of",
                             "csv=p=0", outPath});
    probe.waitForFinished(10000);
    const QString dims = QString::fromUtf8(probe.readAllStandardOutput()).trimmed();
    const QString expected = QStringLiteral("%1,%2").arg(contributedPreset.parameters.value("width").toInt())
                                  .arg(contributedPreset.parameters.value("height").toInt());
    if (dims != expected) {
        fprintf(stderr, "FAILED: plugin preset conversion produced %s, expected %s\n", qPrintable(dims),
                qPrintable(expected));
        return 1;
    }

    fprintf(stderr, "OK: real conversion using the plugin's preset produced %s at %s\n", qPrintable(outPath),
            qPrintable(dims));
    fprintf(stderr, "ALL PLUGIN CHECKS PASSED\n");
    return 0;
}
