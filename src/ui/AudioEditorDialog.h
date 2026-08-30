#pragma once

#include <QDialog>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QTableWidget;
class QSlider;
class QDoubleSpinBox;
class QCheckBox;
class QComboBox;
class QLabel;
QT_END_NAMESPACE

namespace magnify::core {
class JobManager;
}

namespace magnify::ui {

// Audio counterpart of VideoEditorDialog: combine multiple clips (each
// independently trimmed) into one output, with volume, speed, fade in/out,
// and loudness normalization — all through one real ffmpeg filter_complex
// graph (FFmpegMediaEngine::buildAudioEditArgs), not a fake preview.
// Enqueues directly into the JobManager it's given.
class AudioEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit AudioEditorDialog(magnify::core::JobManager *jobManager, QWidget *parent = nullptr);

    // Lets a caller (e.g. a multi-file audio selection) pre-populate the
    // clip list instead of starting empty.
    void addClips(const QStringList &paths);

private:
    void addClipRow(const QString &path);
    void removeSelectedClip();
    void moveSelectedClip(int direction);
    void exportEdit();

    magnify::core::JobManager *m_jobManager = nullptr;

    QTableWidget *m_clipsTable = nullptr;
    QSlider *m_volumeSlider = nullptr;
    QDoubleSpinBox *m_speedSpin = nullptr;
    QDoubleSpinBox *m_fadeInSpin = nullptr;
    QDoubleSpinBox *m_fadeOutSpin = nullptr;
    QCheckBox *m_normalizeCheck = nullptr;
    QComboBox *m_outputFormatCombo = nullptr;
};

} // namespace magnify::ui
