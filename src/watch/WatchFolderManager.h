#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QSet>
#include <QVariantMap>
#include <QVector>

namespace magnify::watch {

struct WatchRule {
    QString folderPath;
    QString targetExt;
    QVariantMap parameters;
    bool enabled = true;
};

// Monitors a set of folders for newly created files and, after a short
// settle delay (to avoid grabbing a file mid-write), reports them for
// conversion according to whichever WatchRule owns that folder. Detection is
// non-recursive, matching QFileSystemWatcher's own granularity.
class WatchFolderManager : public QObject {
    Q_OBJECT
public:
    explicit WatchFolderManager(QObject *parent = nullptr);

    void addRule(const WatchRule &rule);
    void removeRule(const QString &folderPath);
    const QVector<WatchRule> &rules() const { return m_rules; }

signals:
    // Emitted once per new file, after the settle delay, for the caller to
    // enqueue a conversion job with rule.targetExt / rule.parameters.
    void fileDetected(const QString &filePath, const magnify::watch::WatchRule &rule);

private:
    void onDirectoryChanged(const QString &path);

    QFileSystemWatcher m_watcher;
    QVector<WatchRule> m_rules;
    QHash<QString, QSet<QString>> m_knownFiles; // folderPath -> file names last seen there
};

} // namespace magnify::watch
