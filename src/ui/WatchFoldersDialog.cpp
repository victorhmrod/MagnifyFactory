#include "WatchFoldersDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "core/FormatRegistry.h"

using magnify::core::FormatCategory;
using magnify::core::FormatRegistry;
using magnify::watch::WatchFolderManager;
using magnify::watch::WatchRule;

namespace magnify::ui {

namespace {
// Small inline dialog for picking a folder + target format when adding a
// watch rule — there's no source file here to infer a category from, unlike
// ConvertDialog, so category and format are both explicit combos.
class AddWatchRuleDialog : public QDialog {
public:
    explicit AddWatchRuleDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle(QStringLiteral("Add Watch Folder"));
        auto *layout = new QVBoxLayout(this);

        auto *folderRow = new QHBoxLayout();
        m_folderLabel = new QLabel(QStringLiteral("(no folder selected)"), this);
        auto *browseButton = new QPushButton(QStringLiteral("Choose Folder..."), this);
        connect(browseButton, &QPushButton::clicked, this, [this]() {
            const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Folder to watch"));
            if (!dir.isEmpty()) {
                m_folderPath = dir;
                m_folderLabel->setText(dir);
            }
        });
        folderRow->addWidget(m_folderLabel, 1);
        folderRow->addWidget(browseButton);
        layout->addLayout(folderRow);

        auto *categoryRow = new QHBoxLayout();
        categoryRow->addWidget(new QLabel(QStringLiteral("Category:"), this));
        m_categoryCombo = new QComboBox(this);
        m_categoryCombo->addItem(QStringLiteral("Video"), static_cast<int>(FormatCategory::Video));
        m_categoryCombo->addItem(QStringLiteral("Audio"), static_cast<int>(FormatCategory::Audio));
        m_categoryCombo->addItem(QStringLiteral("Image"), static_cast<int>(FormatCategory::Image));
        connect(m_categoryCombo, &QComboBox::currentIndexChanged, this, &AddWatchRuleDialog::refreshFormats);
        categoryRow->addWidget(m_categoryCombo);

        categoryRow->addWidget(new QLabel(QStringLiteral("Convert to:"), this));
        m_formatCombo = new QComboBox(this);
        categoryRow->addWidget(m_formatCombo);
        layout->addLayout(categoryRow);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, &AddWatchRuleDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &AddWatchRuleDialog::reject);
        layout->addWidget(buttons);

        refreshFormats();
    }

    QString folderPath() const { return m_folderPath; }
    QString targetFormat() const { return m_formatCombo->currentData().toString(); }

private:
    void refreshFormats() {
        m_formatCombo->clear();
        const auto category = static_cast<FormatCategory>(m_categoryCombo->currentData().toInt());
        for (const auto &descriptor : FormatRegistry::instance().formatsInCategory(category)) {
            if (descriptor.supportsOutput) {
                m_formatCombo->addItem(descriptor.name, descriptor.extension);
            }
        }
    }

    QString m_folderPath;
    QLabel *m_folderLabel = nullptr;
    QComboBox *m_categoryCombo = nullptr;
    QComboBox *m_formatCombo = nullptr;
};
} // namespace

WatchFoldersDialog::WatchFoldersDialog(WatchFolderManager *manager, QWidget *parent)
    : QDialog(parent), m_manager(manager) {
    setWindowTitle(QStringLiteral("Watch Folders"));
    setMinimumSize(480, 320);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(QStringLiteral("Files added to a watched folder are converted automatically."), this));

    m_list = new QListWidget(this);
    layout->addWidget(m_list, 1);

    auto *buttonsRow = new QHBoxLayout();
    auto *addButton = new QPushButton(QStringLiteral("Add..."), this);
    connect(addButton, &QPushButton::clicked, this, &WatchFoldersDialog::addRule);
    buttonsRow->addWidget(addButton);
    auto *removeButton = new QPushButton(QStringLiteral("Remove Selected"), this);
    connect(removeButton, &QPushButton::clicked, this, &WatchFoldersDialog::removeSelectedRule);
    buttonsRow->addWidget(removeButton);
    buttonsRow->addStretch(1);
    layout->addLayout(buttonsRow);

    auto *closeButtons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(closeButtons, &QDialogButtonBox::rejected, this, &WatchFoldersDialog::reject);
    connect(closeButtons, &QDialogButtonBox::accepted, this, &WatchFoldersDialog::accept);
    layout->addWidget(closeButtons);

    refreshList();
}

void WatchFoldersDialog::refreshList() {
    m_list->clear();
    for (const WatchRule &rule : m_manager->rules()) {
        m_list->addItem(QStringLiteral("%1  →  %2").arg(rule.folderPath, rule.targetExt.toUpper()));
    }
}

void WatchFoldersDialog::addRule() {
    AddWatchRuleDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (dialog.folderPath().isEmpty() || dialog.targetFormat().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Watch Folders"), QStringLiteral("Choose a folder and a format."));
        return;
    }

    WatchRule rule;
    rule.folderPath = dialog.folderPath();
    rule.targetExt = dialog.targetFormat();
    rule.enabled = true;
    m_manager->addRule(rule);
    refreshList();
}

void WatchFoldersDialog::removeSelectedRule() {
    const int row = m_list->currentRow();
    if (row < 0 || row >= m_manager->rules().size()) {
        return;
    }
    m_manager->removeRule(m_manager->rules().at(row).folderPath);
    refreshList();
}

} // namespace magnify::ui
