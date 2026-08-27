#pragma once

#include <QString>
#include <QVector>
#include <QtPlugin>

#include "presets/Preset.h"

namespace magnify::plugins {

// Third-party extension point, loaded dynamically via QPluginLoader from the
// plugins/ folder next to the executable. A plugin is a QObject subclass
// that also implements this interface (Q_INTERFACES) and carries
// Q_PLUGIN_METADATA — see src/plugins/sample/ for a working example.
//
// Extension surface is intentionally narrow for now: presets are plain
// value types, so they cross the plugin/host DLL boundary safely. Handing a
// plugin a live IMediaEngine* (owning QProcess handles, threads, ...) across
// that boundary invites ABI/lifetime problems that are out of scope here;
// that's the natural next extension point once it's needed.
class IMagnifyPlugin {
public:
    virtual ~IMagnifyPlugin() = default;

    virtual QString name() const = 0;
    virtual QString version() const = 0;

    virtual void initialize() = 0;
    virtual void shutdown() = 0;

    // Presets this plugin contributes; merged into PresetRegistry after
    // initialize() runs. Default: none.
    virtual QVector<magnify::presets::Preset> presets() const { return {}; }
};

} // namespace magnify::plugins

#define MagnifyPlugin_iid "com.magnifyfactory.IMagnifyPlugin/1.0"
Q_DECLARE_INTERFACE(magnify::plugins::IMagnifyPlugin, MagnifyPlugin_iid)
