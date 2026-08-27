#pragma once

#include <QDialog>

#include "watch/WatchFolderManager.h"

QT_BEGIN_NAMESPACE
class QListWidget;
QT_END_NAMESPACE

namespace magnify::ui {

// Modal CRUD panel for WatchFolderManager's rules: add a folder + target
// format, remove one, see what's currently being watched. WatchFolderManager
// itself owns the actual monitoring; this dialog only edits its rule list.
class WatchFoldersDialog : public QDialog {
    Q_OBJECT
public:
    explicit WatchFoldersDialog(magnify::watch::WatchFolderManager *manager, QWidget *parent = nullptr);

private:
    void refreshList();
    void addRule();
    void removeSelectedRule();

    magnify::watch::WatchFolderManager *m_manager;
    QListWidget *m_list = nullptr;
};

} // namespace magnify::ui
