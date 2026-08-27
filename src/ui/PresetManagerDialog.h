#pragma once

#include <QDialog>

#include "core/FormatRegistry.h"

QT_BEGIN_NAMESPACE
class QListWidget;
QT_END_NAMESPACE

namespace magnify::ui {

// Lists every preset (built-in and user-created), lets the user create a
// new one (PresetEditorDialog) or delete a user-created one. PresetRegistry
// itself owns persistence; this dialog only drives it.
class PresetManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit PresetManagerDialog(QWidget *parent = nullptr);

private:
    void refreshList();
    void createPreset();
    void deleteSelectedPreset();

    QListWidget *m_list = nullptr;
};

} // namespace magnify::ui
