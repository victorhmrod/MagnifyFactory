#pragma once

#include <QObject>

#include "plugins/IMagnifyPlugin.h"

namespace magnify::plugins::sample {

// Demonstrates the plugin API end to end: a real, separately-built DLL that
// contributes a preset MagnifyFactory's core code never hardcoded. Built as
// its own CMake target (magnify_plugin_sample) into plugins/ next to the
// executable, loaded at runtime by PluginManager — not linked into the app.
class SampleWhatsAppPlugin : public QObject, public IMagnifyPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID MagnifyPlugin_iid FILE "SampleWhatsAppPlugin.json")
    Q_INTERFACES(magnify::plugins::IMagnifyPlugin)

public:
    QString name() const override { return QStringLiteral("Sample WhatsApp Status Preset"); }
    QString version() const override { return QStringLiteral("1.0.0"); }

    void initialize() override;
    void shutdown() override;

    QVector<magnify::presets::Preset> presets() const override;
};

} // namespace magnify::plugins::sample
