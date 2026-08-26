#pragma once

#include <QMainWindow>
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
QT_END_NAMESPACE

namespace magnify::engines::ffmpeg { class FFmpegMediaEngine; }
namespace magnify::engines::pdf { class PdfEngine; }
namespace magnify::core { class JobManager; class ConversionJob; }

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

private:
    void buildUi();
    void applyDarkTheme();
    void addInputFile(const QString &filePath);
    void enqueueFile(const QString &inputPath, const QString &targetExt);
    void refreshRow(magnify::core::ConversionJob *job);
    void appendRow(magnify::core::ConversionJob *job);
    void onCategorySelected(QListWidgetItem *current);
    void updateStatusBar();

    std::unique_ptr<magnify::engines::ffmpeg::FFmpegMediaEngine> m_ffmpegEngine;
    std::unique_ptr<magnify::engines::pdf::PdfEngine> m_pdfEngine;
    std::unique_ptr<magnify::core::JobManager> m_jobManager;

    QListWidget *m_sidebar = nullptr;
    QTableWidget *m_queueTable = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_dropZoneButton = nullptr;
    QSpinBox *m_concurrencySpin = nullptr;
    QComboBox *m_hardwareCombo = nullptr;
    QLabel *m_statusJobsLabel = nullptr;

    magnify::core::FormatCategory m_activeCategory = magnify::core::FormatCategory::Video;
};

} // namespace magnify::ui
