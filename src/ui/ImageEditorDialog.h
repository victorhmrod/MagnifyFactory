#pragma once

#include <QDialog>
#include <QImage>
#include <QString>

QT_BEGIN_NAMESPACE
class QLabel;
class QSlider;
class QSpinBox;
class QCheckBox;
class QLineEdit;
class QComboBox;
QT_END_NAMESPACE

namespace magnify::core {
class JobManager;
}

namespace magnify::ui {

class CropLabel;

// First image-editing surface: crop (drag a region right on the displayed
// image), resize, color/blur adjustments, and a text overlay — all shown
// live via in-app QImage compositing for interactivity, then re-applied for
// real by ffmpeg (crop/scale/eq/gblur/drawtext, via
// FFmpegMediaEngine::buildImageEditArgs) on Export, so what ships is
// genuinely processed, not just what the preview approximated.
class ImageEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit ImageEditorDialog(const QString &filePath, magnify::core::JobManager *jobManager,
                                QWidget *parent = nullptr);

private:
    void updatePreview();
    void exportEdit();

    magnify::core::JobManager *m_jobManager = nullptr;
    QString m_sourcePath;
    QImage m_sourceImage;
    int m_sourceWidth = 0;
    int m_sourceHeight = 0;

    CropLabel *m_cropLabel = nullptr;
    QLabel *m_cropInfoLabel = nullptr;
    QSpinBox *m_resizeWidthSpin = nullptr;
    QSpinBox *m_resizeHeightSpin = nullptr;
    QCheckBox *m_keepAspectCheck = nullptr;
    QSlider *m_brightnessSlider = nullptr;
    QSlider *m_contrastSlider = nullptr;
    QSlider *m_saturationSlider = nullptr;
    QSlider *m_blurSlider = nullptr;
    QLineEdit *m_overlayTextEdit = nullptr;
    QComboBox *m_overlayPositionCombo = nullptr;
    QSpinBox *m_overlayFontSizeSpin = nullptr;
    QComboBox *m_outputFormatCombo = nullptr;

    bool m_updatingResizeFields = false;
};

} // namespace magnify::ui
