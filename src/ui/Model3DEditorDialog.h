#pragma once

#include <QDialog>
#include <QString>

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QCheckBox;
class QComboBox;
QT_END_NAMESPACE

namespace magnify::core {
class JobManager;
}

namespace magnify::ui {

// 3D model transform + conversion editor: scale, rotate (X/Y/Z degrees),
// recenter the origin, and reduce polygon count (decimate), all applied for
// real by Blender headless on export (Model3DEngine). Unlike the
// Video/Image/Audio editors, there is no in-app preview — a real 3D
// viewport is a project of its own, out of scope for now — this is a
// numeric-parameter form only, but the export is still genuine Blender
// processing, not a fake result.
class Model3DEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit Model3DEditorDialog(const QString &filePath, magnify::core::JobManager *jobManager,
                                  QWidget *parent = nullptr);

private:
    void exportEdit();

    magnify::core::JobManager *m_jobManager = nullptr;
    QString m_inputPath;

    QDoubleSpinBox *m_scaleSpin = nullptr;
    QDoubleSpinBox *m_rotateXSpin = nullptr;
    QDoubleSpinBox *m_rotateYSpin = nullptr;
    QDoubleSpinBox *m_rotateZSpin = nullptr;
    QCheckBox *m_centerCheck = nullptr;
    QDoubleSpinBox *m_decimateSpin = nullptr;
    QComboBox *m_outputFormatCombo = nullptr;
};

} // namespace magnify::ui
