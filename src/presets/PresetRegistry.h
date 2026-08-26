#pragma once

#include <QVector>

#include "Preset.h"

namespace magnify::presets {

// Central catalogue of presets, mirroring how FormatRegistry centralizes
// formats. Ships with built-in presets and additionally loads any *.json
// preset files a user drops into the presets/ folder next to the
// executable, so presets can be shared/exported as plain files.
class PresetRegistry {
public:
    static PresetRegistry &instance();

    QVector<Preset> presetsForCategory(magnify::core::FormatCategory category) const;
    const QVector<Preset> &all() const { return m_presets; }

private:
    PresetRegistry();
    void registerBuiltins();
    void loadUserPresets();

    QVector<Preset> m_presets;
};

} // namespace magnify::presets
