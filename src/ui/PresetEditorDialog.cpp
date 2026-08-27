#include "PresetEditorDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>
#include <QVBoxLayout>

#include "core/FormatRegistry.h"

using magnify::core::FormatCategory;
using magnify::core::FormatRegistry;
using magnify::presets::Preset;

namespace magnify::ui {

namespace {
QString categoryLabel(FormatCategory category) {
    switch (category) {
        case FormatCategory::Video: return QStringLiteral("Video");
        case FormatCategory::Audio: return QStringLiteral("Audio");
        case FormatCategory::Image: return QStringLiteral("Image");
        case FormatCategory::Pdf: return QStringLiteral("PDF");
        default: return QString();
    }
}
} // namespace

PresetEditorDialog::PresetEditorDialog(FormatCategory defaultCategory, QWidget *parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("New Preset"));
    setModal(true);
    setMinimumWidth(360);

    auto *layout = new QVBoxLayout(this);
    auto *topForm = new QFormLayout();

    m_nameEdit = new QLineEdit(this);
    topForm->addRow(QStringLiteral("Name:"), m_nameEdit);

    m_categoryCombo = new QComboBox(this);
    for (FormatCategory category : {FormatCategory::Video, FormatCategory::Audio, FormatCategory::Image,
                                     FormatCategory::Pdf}) {
        m_categoryCombo->addItem(categoryLabel(category), static_cast<int>(category));
    }
    const int defaultIndex = m_categoryCombo->findData(static_cast<int>(defaultCategory));
    m_categoryCombo->setCurrentIndex(defaultIndex >= 0 ? defaultIndex : 0);
    topForm->addRow(QStringLiteral("Category:"), m_categoryCombo);

    m_formatCombo = new QComboBox(this);
    topForm->addRow(QStringLiteral("Target format:"), m_formatCombo);

    layout->addLayout(topForm);

    m_paramContainer = new QWidget(this);
    m_paramForm = new QFormLayout(m_paramContainer);
    m_paramForm->setContentsMargins(0, 8, 0, 0);
    layout->addWidget(m_paramContainer);

    connect(m_categoryCombo, &QComboBox::currentIndexChanged, this, [this]() {
        const auto category = static_cast<FormatCategory>(m_categoryCombo->currentData().toInt());
        m_formatCombo->clear();
        for (const auto &descriptor : FormatRegistry::instance().formatsInCategory(category)) {
            if (descriptor.supportsOutput) {
                m_formatCombo->addItem(descriptor.name, descriptor.extension);
            }
        }
        rebuildParameterFields();
    });
    connect(m_formatCombo, &QComboBox::currentIndexChanged, this, [this]() { rebuildParameterFields(); });

    emit m_categoryCombo->currentIndexChanged(m_categoryCombo->currentIndex());

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &PresetEditorDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void PresetEditorDialog::rebuildParameterFields() {
    while (m_paramForm->rowCount() > 0) {
        m_paramForm->removeRow(0);
    }
    m_qualitySpin = nullptr;
    m_widthSpin = nullptr;
    m_heightSpin = nullptr;
    m_videoBitrateEdit = nullptr;
    m_audioBitrateEdit = nullptr;

    const auto category = static_cast<FormatCategory>(m_categoryCombo->currentData().toInt());
    const QString targetExt = m_formatCombo->currentData().toString();

    if (category == FormatCategory::Video) {
        m_qualitySpin = new QSpinBox(m_paramContainer);
        m_qualitySpin->setRange(0, 51);
        m_qualitySpin->setValue(23);
        m_paramForm->addRow(QStringLiteral("Quality (CRF, lower = better):"), m_qualitySpin);

        m_widthSpin = new QSpinBox(m_paramContainer);
        m_widthSpin->setRange(0, 7680);
        m_widthSpin->setSpecialValueText(QStringLiteral("keep source"));
        m_paramForm->addRow(QStringLiteral("Width (upscale/downscale):"), m_widthSpin);

        m_heightSpin = new QSpinBox(m_paramContainer);
        m_heightSpin->setRange(0, 4320);
        m_heightSpin->setSpecialValueText(QStringLiteral("keep source"));
        m_paramForm->addRow(QStringLiteral("Height (upscale/downscale):"), m_heightSpin);

        m_videoBitrateEdit = new QLineEdit(m_paramContainer);
        m_videoBitrateEdit->setPlaceholderText(QStringLiteral("e.g. 6000k (hardware encoders only)"));
        m_paramForm->addRow(QStringLiteral("Video bitrate:"), m_videoBitrateEdit);

        m_audioBitrateEdit = new QLineEdit(m_paramContainer);
        m_audioBitrateEdit->setText(QStringLiteral("192k"));
        m_paramForm->addRow(QStringLiteral("Audio bitrate:"), m_audioBitrateEdit);
    } else if (category == FormatCategory::Audio) {
        if (targetExt == QStringLiteral("mp3") || targetExt == QStringLiteral("aac") ||
            targetExt == QStringLiteral("m4a")) {
            m_audioBitrateEdit = new QLineEdit(m_paramContainer);
            m_audioBitrateEdit->setPlaceholderText(QStringLiteral("e.g. 320k — leave blank for default quality"));
            m_paramForm->addRow(QStringLiteral("Audio bitrate:"), m_audioBitrateEdit);
        } else {
            m_paramForm->addRow(new QLabel(QStringLiteral("Lossless format — nothing to tune."), m_paramContainer));
        }
    } else if (category == FormatCategory::Image) {
        if (targetExt == QStringLiteral("webp")) {
            m_qualitySpin = new QSpinBox(m_paramContainer);
            m_qualitySpin->setRange(0, 100);
            m_qualitySpin->setValue(80);
            m_paramForm->addRow(QStringLiteral("Quality (higher = better):"), m_qualitySpin);
        } else if (targetExt == QStringLiteral("jpg") || targetExt == QStringLiteral("jpeg")) {
            m_qualitySpin = new QSpinBox(m_paramContainer);
            m_qualitySpin->setRange(2, 31);
            m_qualitySpin->setValue(5);
            m_paramForm->addRow(QStringLiteral("Quality (2 = best, 31 = worst):"), m_qualitySpin);
        } else if (targetExt == QStringLiteral("avif")) {
            m_qualitySpin = new QSpinBox(m_paramContainer);
            m_qualitySpin->setRange(0, 63);
            m_qualitySpin->setValue(30);
            m_paramForm->addRow(QStringLiteral("CRF (0 = best, 63 = worst):"), m_qualitySpin);
        } else {
            m_paramForm->addRow(new QLabel(QStringLiteral("No adjustable quality for this format."), m_paramContainer));
        }
    } else if (category == FormatCategory::Pdf) {
        m_qualitySpin = new QSpinBox(m_paramContainer);
        m_qualitySpin->setRange(0, 100);
        m_qualitySpin->setValue(60);
        m_paramForm->addRow(QStringLiteral("Compressed image quality:"), m_qualitySpin);
    }
}

void PresetEditorDialog::onAccepted() {
    const QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("New Preset"), QStringLiteral("Give the preset a name."));
        return;
    }
    if (m_formatCombo->currentIndex() < 0) {
        QMessageBox::warning(this, QStringLiteral("New Preset"), QStringLiteral("No target format available."));
        return;
    }

    const auto category = static_cast<FormatCategory>(m_categoryCombo->currentData().toInt());
    const QString targetExt = m_formatCombo->currentData().toString();

    QVariantMap params;
    if (category == FormatCategory::Video) {
        params[QStringLiteral("crf")] = m_qualitySpin->value();
        if (m_widthSpin->value() > 0 && m_heightSpin->value() > 0) {
            params[QStringLiteral("width")] = m_widthSpin->value();
            params[QStringLiteral("height")] = m_heightSpin->value();
        }
        if (!m_videoBitrateEdit->text().trimmed().isEmpty()) {
            params[QStringLiteral("videoBitrate")] = m_videoBitrateEdit->text().trimmed();
        }
        if (!m_audioBitrateEdit->text().trimmed().isEmpty()) {
            params[QStringLiteral("audioBitrate")] = m_audioBitrateEdit->text().trimmed();
        }
    } else if (category == FormatCategory::Audio) {
        if (m_audioBitrateEdit && !m_audioBitrateEdit->text().trimmed().isEmpty()) {
            params[QStringLiteral("audioBitrate")] = m_audioBitrateEdit->text().trimmed();
        }
    } else if (category == FormatCategory::Image) {
        if (m_qualitySpin) {
            if (targetExt == QStringLiteral("webp")) {
                params[QStringLiteral("quality")] = m_qualitySpin->value();
            } else if (targetExt == QStringLiteral("jpg") || targetExt == QStringLiteral("jpeg")) {
                params[QStringLiteral("jpegQuality")] = m_qualitySpin->value();
            } else if (targetExt == QStringLiteral("avif")) {
                params[QStringLiteral("crf")] = m_qualitySpin->value();
            }
        }
    } else if (category == FormatCategory::Pdf) {
        params[QStringLiteral("jpegQuality")] = m_qualitySpin->value();
    }

    m_result = Preset{name, category, targetExt, params, false, QString()};
    accept();
}

} // namespace magnify::ui
