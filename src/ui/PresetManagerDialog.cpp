#include "PresetManagerDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "PresetEditorDialog.h"
#include "presets/PresetRegistry.h"

using magnify::core::FormatCategory;
using magnify::presets::Preset;
using magnify::presets::PresetRegistry;

namespace magnify::ui {

namespace {
QString categoryLabel(FormatCategory category) {
    switch (category) {
        case FormatCategory::Video: return QStringLiteral("Video");
        case FormatCategory::Audio: return QStringLiteral("Audio");
        case FormatCategory::Image: return QStringLiteral("Image");
        case FormatCategory::Pdf: return QStringLiteral("PDF");
        default: return QStringLiteral("Other");
    }
}
} // namespace

PresetManagerDialog::PresetManagerDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("Presets"));
    setMinimumSize(480, 360);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        QStringLiteral("Built-in presets can't be deleted. Presets you create live in presets/*.json "
                        "next to the executable."),
        this));

    m_list = new QListWidget(this);
    layout->addWidget(m_list, 1);

    auto *buttonsRow = new QHBoxLayout();
    auto *newButton = new QPushButton(QStringLiteral("New Preset..."), this);
    connect(newButton, &QPushButton::clicked, this, &PresetManagerDialog::createPreset);
    buttonsRow->addWidget(newButton);
    auto *deleteButton = new QPushButton(QStringLiteral("Delete Selected"), this);
    connect(deleteButton, &QPushButton::clicked, this, &PresetManagerDialog::deleteSelectedPreset);
    buttonsRow->addWidget(deleteButton);
    buttonsRow->addStretch(1);
    layout->addLayout(buttonsRow);

    auto *closeButtons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(closeButtons, &QDialogButtonBox::rejected, this, &PresetManagerDialog::reject);
    connect(closeButtons, &QDialogButtonBox::accepted, this, &PresetManagerDialog::accept);
    layout->addWidget(closeButtons);

    refreshList();
}

void PresetManagerDialog::refreshList() {
    m_list->clear();
    for (const Preset &preset : PresetRegistry::instance().all()) {
        const QString suffix = preset.isUserDefined ? QString() : QStringLiteral(" [built-in]");
        auto *item = new QListWidgetItem(QStringLiteral("%1  —  %2 → %3%4")
                                              .arg(preset.name, categoryLabel(preset.category),
                                                   preset.targetFormat.toUpper(), suffix));
        item->setData(Qt::UserRole, preset.name);
        item->setData(Qt::UserRole + 1, preset.isUserDefined);
        m_list->addItem(item);
    }
}

void PresetManagerDialog::createPreset() {
    PresetEditorDialog dialog(FormatCategory::Video, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    PresetRegistry::instance().addUserPreset(dialog.result());
    refreshList();
}

void PresetManagerDialog::deleteSelectedPreset() {
    QListWidgetItem *item = m_list->currentItem();
    if (!item) {
        return;
    }
    if (!item->data(Qt::UserRole + 1).toBool()) {
        QMessageBox::information(this, QStringLiteral("Presets"), QStringLiteral("Built-in presets can't be deleted."));
        return;
    }
    const QString name = item->data(Qt::UserRole).toString();
    if (PresetRegistry::instance().removeUserPreset(name)) {
        refreshList();
    }
}

} // namespace magnify::ui
