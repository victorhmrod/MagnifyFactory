#pragma once

#include <QString>
#include <QVector>

#include "IMagnifyPlugin.h"

QT_BEGIN_NAMESPACE
class QPluginLoader;
QT_END_NAMESPACE

namespace magnify::plugins {

// Discovers and loads every Qt plugin (.dll on Windows) in a directory that
// implements IMagnifyPlugin, calling initialize() on each and merging its
// contributed presets into PresetRegistry. Plugins stay loaded (and their
// shutdown() gets called) until the manager itself is destroyed.
class PluginManager {
public:
    ~PluginManager();

    // Non-existent directories are simply skipped (no error) — plugins are
    // optional, not a startup requirement.
    void loadPluginsFrom(const QString &directory);

    struct LoadedPlugin {
        IMagnifyPlugin *plugin;
        QString filePath;
    };
    const QVector<LoadedPlugin> &loadedPlugins() const { return m_loaded; }

private:
    QVector<QPluginLoader *> m_loaders;
    QVector<LoadedPlugin> m_loaded;
};

} // namespace magnify::plugins
