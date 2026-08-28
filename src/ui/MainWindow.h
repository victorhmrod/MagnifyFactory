#pragma once

#include <QFutureWatcher>
#include <QMainWindow>
#include <QVariantMap>
#include <memory>

#include "core/FormatRegistry.h"

QT_BEGIN_NAMESPACE
class QTableWidget;
class QComboBox;
class QLineEdit;
class QPushButton;
class QLabel;
class QSpinBox;
class QListWidget;
class QListWidgetItem;
class QStackedWidget;
class QCloseEvent;
QT_END_NAMESPACE

namespace magnify::engines::ffmpeg { class FFmpegMediaEngine; }
namespace magnify::engines::pdf { class PdfEngine; }
namespace magnify::engines::archive { class ArchiveEngine; }
namespace magnify::engines::document { class DocumentEngine; }
namespace magnify::core { class JobManager; class ConversionJob; }
namespace magnify::watch { class WatchFolderManager; struct WatchRule; }
namespace magnify::plugins { class PluginManager; }

namespace magnify::ui {

// Top-level window. Contains no conversion logic itself — it only translates
// user actions (drag-and-drop, popup format selection, sidebar navigation)
// into JobManager calls, and reflects ConversionJob state back into the
// queue table.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // Entry point for files handed in from outside the UI (CLI argument from
    // the Windows "Convert with MagnifyFactory" context menu integration).
    void openExternalFile(const QString &filePath);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    void applyDarkTheme();
    void addInputFile(const QString &filePath);
    // Groups files by format category and shows one ConvertDialog per group
    // (not one per file) so batch drops/folder adds pick a format once and
    // apply it to every matching file.
    void addInputFiles(const QStringList &paths);
    void addInputFolder();
    void enqueueFile(const QString &inputPath, const QString &targetExt, const QVariantMap &presetParameters = {});
    // Merges PDFs and/or images (any mix) into one PDF — each image becomes
    // a page, via PdfEngine's merge operation.
    void mergeIntoPdf(const QStringList &files);
    void compressToZip(const QStringList &files);
    void extractArchive(const QString &archivePath);
    void refreshRow(magnify::core::ConversionJob *job);
    void appendRow(magnify::core::ConversionJob *job);
    void onCategorySelected(QListWidgetItem *current);
    void updateStatusBar();
    void populateHardwareCombo();
    void onHardwareDetectionFinished();
    void openWatchFoldersDialog();
    void openPresetManagerDialog();
    void onWatchedFileDetected(const QString &filePath, const magnify::watch::WatchRule &rule);
    void loadSettings();
    void saveSettings();
    void showQueueContextMenu(const QPoint &pos);
    void showMediaInfo(const QUuid &jobId);
    void onQueueRowsMoved();
    void removeRowForJob(const QUuid &jobId);

    std::unique_ptr<magnify::engines::ffmpeg::FFmpegMediaEngine> m_ffmpegEngine;
    std::unique_ptr<magnify::engines::pdf::PdfEngine> m_pdfEngine;
    std::unique_ptr<magnify::engines::archive::ArchiveEngine> m_archiveEngine;
    std::unique_ptr<magnify::engines::document::DocumentEngine> m_documentEngine;
    std::unique_ptr<magnify::core::JobManager> m_jobManager;
    std::unique_ptr<magnify::watch::WatchFolderManager> m_watchFolderManager;
    std::unique_ptr<magnify::plugins::PluginManager> m_pluginManager;

    QListWidget *m_sidebar = nullptr;
    QTableWidget *m_queueTable = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_dropZoneButton = nullptr;
    QSpinBox *m_concurrencySpin = nullptr;
    QComboBox *m_hardwareCombo = nullptr;
    QLabel *m_statusJobsLabel = nullptr;
    QFutureWatcher<void> m_hardwareDetectionWatcher;

    magnify::core::FormatCategory m_activeCategory = magnify::core::FormatCategory::Video;
    QString m_pendingHardwareBackend; // restored from settings, applied once the combo is fully populated
};

} // namespace magnify::ui
