#pragma once

#include <QDialog>

#include "presets/Preset.h"

QT_BEGIN_NAMESPACE
class QComboBox;
class QLineEdit;
class QSpinBox;
class QFormLayout;
class QWidget;
QT_END_NAMESPACE

namespace magnify::ui {

// Form for creating a new preset: name, category, target format, and a
// handful of category-relevant parameters (quality/CRF, resolution —
// entering a size larger than the source is how you ask for upscaling,
// bitrate). Saving hands a fully-formed Preset back to the caller, which is
// responsible for actually registering/persisting it (PresetRegistry).
class PresetEditorDialog : public QDialog {
    Q_OBJECT
public:
    // defaultCategory pre-selects the category (e.g. the file the user was
    // about to convert when they opened this), still changeable in the form.
    explicit PresetEditorDialog(magnify::core::FormatCategory defaultCategory, QWidget *parent = nullptr);

    magnify::presets::Preset result() const { return m_result; }

private:
    void rebuildParameterFields();
    void onAccepted();

    QComboBox *m_categoryCombo = nullptr;
    QComboBox *m_formatCombo = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QFormLayout *m_paramForm = nullptr;
    QWidget *m_paramContainer = nullptr;

    // One or more of these are populated depending on the selected category;
    // unused ones stay null.
    QSpinBox *m_qualitySpin = nullptr; // CRF (video) or 0-100 quality (image/pdf)
    QSpinBox *m_widthSpin = nullptr;
    QSpinBox *m_heightSpin = nullptr;
    QLineEdit *m_videoBitrateEdit = nullptr;
    QLineEdit *m_audioBitrateEdit = nullptr;

    magnify::presets::Preset m_result;
};

} // namespace magnify::ui
