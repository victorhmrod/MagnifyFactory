#include "SampleWhatsAppPlugin.h"

using magnify::core::FormatCategory;
using magnify::presets::Preset;

namespace magnify::plugins::sample {

void SampleWhatsAppPlugin::initialize() {
    // Nothing to set up for a preset-only plugin; a plugin that registered
    // its own engine or opened resources would do that here.
}

void SampleWhatsAppPlugin::shutdown() {
}

QVector<Preset> SampleWhatsAppPlugin::presets() const {
    return {
        Preset{QStringLiteral("Plugin: WhatsApp Status"), FormatCategory::Video, QStringLiteral("mp4"),
               {{"crf", 28}, {"width", 720}, {"height", 1280}, {"audioBitrate", "96k"}}},
    };
}

} // namespace magnify::plugins::sample
