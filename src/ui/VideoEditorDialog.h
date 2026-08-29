#pragma once

#include <QDialog>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QTableWidget;
class QSlider;
class QDoubleSpinBox;
class QSpinBox;
class QLineEdit;
class QComboBox;
class QLabel;
QT_END_NAMESPACE

namespace magnify::core {
class JobManager;
}

namespace magnify::ui {

// First slice of the "media factory" video editor: combine multiple clips
// (each independently trimmed) into one output, with global color grading
// (brightness/contrast/saturation), speed, and a burned-in text overlay —
// all through one real ffmpeg filter_complex graph
// (FFmpegMediaEngine::buildVideoEditArgs), not a fake preview. Enqueues
// directly into the JobManager it's given, same pattern WatchFoldersDialog
// uses for its manager.
class VideoEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit VideoEditorDialog(magnify::core::JobManager *jobManager, QWidget *parent = nullptr);

    // Lets a caller (e.g. a multi-file video selection) pre-populate the
    // clip list instead of starting empty.
    void addClips(const QStringList &paths);

private:
    void addClipRow(const QString &path);
    void removeSelectedClip();
    void moveSelectedClip(int direction);
    void exportEdit();

    magnify::core::JobManager *m_jobManager = nullptr;

    QTableWidget *m_clipsTable = nullptr;
    QSlider *m_brightnessSlider = nullptr;
    QSlider *m_contrastSlider = nullptr;
    QSlider *m_saturationSlider = nullptr;
    QDoubleSpinBox *m_speedSpin = nullptr;
    QLineEdit *m_overlayTextEdit = nullptr;
    QComboBox *m_overlayPositionCombo = nullptr;
    QSpinBox *m_overlayFontSizeSpin = nullptr;
    QComboBox *m_outputFormatCombo = nullptr;
};

} // namespace magnify::ui
