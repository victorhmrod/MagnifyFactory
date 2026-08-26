#include "ConvertDialog.h"

#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "presets/PresetRegistry.h"

using magnify::core::FormatCategory;
using magnify::core::FormatRegistry;
using magnify::presets::Preset;
using magnify::presets::PresetRegistry;

namespace magnify::ui {

ConvertDialog::ConvertDialog(const QString &fileName, FormatCategory sourceCategory, QWidget *parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("Convert"));
    setModal(true);
    setMinimumWidth(420);
    setStyleSheet(R"(
        QDialog { background-color: #1e2126; color: #c9d1d9; }
        QLabel#title { font-size: 15px; font-weight: 600; color: #ffffff; }
        QLabel#section { color: #8b949e; font-size: 11px; font-weight: 600; margin-top: 6px; }
        QPushButton[class="formatButton"] {
            background-color: #2d333b; border: 1px solid #3a4048; border-radius: 6px;
            padding: 10px; font-weight: 600; min-width: 64px;
        }
        QPushButton[class="formatButton"]:hover { background-color: #2f6feb; border-color: #2f6feb; color: white; }
    )");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(4);

    auto *title = new QLabel(QStringLiteral("Convert \"%1\" to:").arg(QFileInfo(fileName).fileName()), this);
    title->setObjectName(QStringLiteral("title"));
    title->setWordWrap(true);
    layout->addWidget(title);

    // Offer the source's own category first, plus sensible cross-category
    // targets (e.g. a video can be converted straight to an audio format).
    switch (sourceCategory) {
        case FormatCategory::Video:
            addPresetSection(layout, FormatCategory::Video);
            addFormatSection(layout, QStringLiteral("VIDEO"), FormatCategory::Video);
            addFormatSection(layout, QStringLiteral("EXTRACT AUDIO"), FormatCategory::Audio);
            break;
        case FormatCategory::Audio:
            addPresetSection(layout, FormatCategory::Audio);
            addFormatSection(layout, QStringLiteral("AUDIO"), FormatCategory::Audio);
            break;
        case FormatCategory::Image:
            addPresetSection(layout, FormatCategory::Image);
            addFormatSection(layout, QStringLiteral("IMAGE"), FormatCategory::Image);
            addCustomSection(layout, QStringLiteral("DOCUMENT"), {{QStringLiteral("PDF"), QStringLiteral("pdf")}});
            break;
        case FormatCategory::Pdf:
            // pdftoppm (the renderer behind PDF -> image) only supports these
            // two output formats; the general Image category list includes
            // WebP/AVIF/etc. which it cannot produce.
            addCustomSection(layout, QStringLiteral("IMAGE (page 1)"),
                              {{QStringLiteral("PNG"), QStringLiteral("png")},
                               {QStringLiteral("JPEG"), QStringLiteral("jpg")}});
            // Same extension in and out — PdfEngine detects this pdf->pdf
            // case and runs a qpdf compression pass instead of rendering.
            addCustomSection(layout, QStringLiteral("TOOLS"),
                              {{QStringLiteral("Compress"), QStringLiteral("pdf")}});
            addPresetSection(layout, FormatCategory::Pdf);
            break;
        default:
            addFormatSection(layout, QStringLiteral("VIDEO"), FormatCategory::Video);
            addFormatSection(layout, QStringLiteral("AUDIO"), FormatCategory::Audio);
            addFormatSection(layout, QStringLiteral("IMAGE"), FormatCategory::Image);
            break;
    }
}

void ConvertDialog::addFormatSection(QVBoxLayout *layout, const QString &title, FormatCategory category) {
    QVector<QPair<QString, QString>> formats;
    for (const auto &descriptor : FormatRegistry::instance().formatsInCategory(category)) {
        if (descriptor.supportsOutput) {
            formats.append({descriptor.name, descriptor.extension});
        }
    }
    addCustomSection(layout, title, formats);
}

void ConvertDialog::addPresetSection(QVBoxLayout *layout, FormatCategory category) {
    const QVector<Preset> presets = PresetRegistry::instance().presetsForCategory(category);
    if (presets.isEmpty()) {
        return;
    }

    auto *sectionLabel = new QLabel(QStringLiteral("PRESETS"), this);
    sectionLabel->setObjectName(QStringLiteral("section"));
    layout->addWidget(sectionLabel);

    auto *grid = new QGridLayout();
    grid->setSpacing(8);
    int col = 0;
    int row = 0;
    constexpr int columns = 3;
    for (const Preset &preset : presets) {
        auto *button = new QPushButton(preset.name, this);
        button->setProperty("class", QStringLiteral("formatButton"));
        button->setCursor(Qt::PointingHandCursor);
        connect(button, &QPushButton::clicked, this, [this, preset]() {
            m_selectedFormat = preset.targetFormat;
            m_selectedParameters = preset.parameters;
            accept();
        });
        grid->addWidget(button, row, col);
        if (++col >= columns) {
            col = 0;
            ++row;
        }
    }
    layout->addLayout(grid);
}

void ConvertDialog::addCustomSection(QVBoxLayout *layout, const QString &title,
                                      const QVector<QPair<QString, QString>> &formats) {
    if (formats.isEmpty()) {
        return;
    }

    auto *sectionLabel = new QLabel(title, this);
    sectionLabel->setObjectName(QStringLiteral("section"));
    layout->addWidget(sectionLabel);

    auto *grid = new QGridLayout();
    grid->setSpacing(8);
    int col = 0;
    int row = 0;
    constexpr int columns = 5;
    for (const auto &[name, ext] : formats) {
        auto *button = new QPushButton(name, this);
        button->setProperty("class", QStringLiteral("formatButton"));
        button->setCursor(Qt::PointingHandCursor);
        connect(button, &QPushButton::clicked, this, [this, ext]() {
            m_selectedFormat = ext;
            accept();
        });
        grid->addWidget(button, row, col);
        if (++col >= columns) {
            col = 0;
            ++row;
        }
    }
    layout->addLayout(grid);
}

} // namespace magnify::ui
