#include "PluginManager.h"

#include <QDir>
#include <QLibrary>
#include <QPluginLoader>

#include "presets/PresetRegistry.h"

using magnify::presets::PresetRegistry;

namespace magnify::plugins {

PluginManager::~PluginManager() {
    for (const LoadedPlugin &loaded : m_loaded) {
        loaded.plugin->shutdown();
    }
    for (QPluginLoader *loader : m_loaders) {
        loader->unload();
        delete loader;
    }
}

void PluginManager::loadPluginsFrom(const QString &directory) {
    const QDir dir(directory);
    if (!dir.exists()) {
        return;
    }

    for (const QString &fileName : dir.entryList(QDir::Files)) {
        if (!QLibrary::isLibrary(fileName)) {
            continue;
        }
        auto *loader = new QPluginLoader(dir.filePath(fileName));
        auto *plugin = qobject_cast<IMagnifyPlugin *>(loader->instance());
        if (!plugin) {
            // Not a MagnifyFactory plugin (wrong IID, or just some other
            // DLL sitting in the folder) — not an error, just skip it.
            loader->unload();
            delete loader;
            continue;
        }

        plugin->initialize();
        for (const auto &preset : plugin->presets()) {
            PresetRegistry::instance().registerPreset(preset);
        }

        m_loaders << loader;
        m_loaded << LoadedPlugin{plugin, dir.filePath(fileName)};
    }
}

} // namespace magnify::plugins
