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

    // For plugins (via PluginManager) and anything else contributing presets
    // at runtime, distinct from the built-in/user-JSON presets loaded at
    // construction.
    void registerPreset(const Preset &preset) { m_presets << preset; }

    // Creates a user preset from the Preset Manager UI: registers it in
    // memory immediately and writes it to presets/<name>.json next to the
    // executable, using the same schema loadUserPresets() reads — so it's
    // still there next launch, and remains a plain, inspectable/shareable
    // file like any hand-dropped preset.
    void addUserPreset(Preset preset);
    // Only removes presets with isUserDefined == true (and deletes their
    // backing file); returns false if no matching user preset was found.
    bool removeUserPreset(const QString &name);

private:
    PresetRegistry();
    void registerBuiltins();
    void loadUserPresets();

    QVector<Preset> m_presets;
};

} // namespace magnify::presets
