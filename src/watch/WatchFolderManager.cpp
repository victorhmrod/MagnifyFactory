#include "WatchFolderManager.h"

#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <algorithm>

namespace magnify::watch {

namespace {
constexpr int SettleDelayMs = 2000; // let the writer finish before converting
}

WatchFolderManager::WatchFolderManager(QObject *parent) : QObject(parent) {
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &WatchFolderManager::onDirectoryChanged);
}

void WatchFolderManager::addRule(const WatchRule &rule) {
    const QString canonical = QDir(rule.folderPath).absolutePath();
    WatchRule normalized = rule;
    normalized.folderPath = canonical;
    m_rules.append(normalized);

    if (!m_watcher.directories().contains(canonical)) {
        m_watcher.addPath(canonical);
        // Snapshot the folder's current contents so pre-existing files
        // aren't treated as "new" the moment watching starts.
        m_knownFiles[canonical] = QSet<QString>(QDir(canonical).entryList(QDir::Files).cbegin(),
                                                 QDir(canonical).entryList(QDir::Files).cend());
    }
}

void WatchFolderManager::removeRule(const QString &folderPath) {
    const QString canonical = QDir(folderPath).absolutePath();
    m_rules.removeIf([&](const WatchRule &rule) { return rule.folderPath == canonical; });

    const bool stillNeeded =
        std::any_of(m_rules.cbegin(), m_rules.cend(), [&](const WatchRule &rule) { return rule.folderPath == canonical; });
    if (!stillNeeded) {
        m_watcher.removePath(canonical);
        m_knownFiles.remove(canonical);
    }
}

void WatchFolderManager::onDirectoryChanged(const QString &path) {
    const QDir dir(path);
    const auto entryList = dir.entryList(QDir::Files);
    const QSet<QString> currentFiles(entryList.cbegin(), entryList.cend());
    QSet<QString> &known = m_knownFiles[path];
    const QSet<QString> newFiles = currentFiles - known;
    known = currentFiles;

    for (const QString &fileName : newFiles) {
        const QString fullPath = dir.filePath(fileName);
        QTimer::singleShot(SettleDelayMs, this, [this, fullPath, path]() {
            if (!QFileInfo::exists(fullPath)) {
                return; // moved/deleted during the settle delay
            }
            for (const WatchRule &rule : m_rules) {
                if (rule.enabled && rule.folderPath == path) {
                    emit fileDetected(fullPath, rule);
                }
            }
        });
    }
}

} // namespace magnify::watch
