#include "Model3DEditorDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "core/ConversionJob.h"
#include "core/JobManager.h"

using magnify::core::ConversionJob;
using magnify::core::JobManager;

namespace magnify::ui {

Model3DEditorDialog::Model3DEditorDialog(const QString &filePath, JobManager *jobManager, QWidget *parent)
    : QDialog(parent), m_jobManager(jobManager), m_inputPath(filePath) {
    setWindowTitle(QStringLiteral("3D Model Editor"));
    setMinimumWidth(420);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(QStringLiteral("Editing: %1").arg(QFileInfo(filePath).fileName()), this));
    layout->addWidget(new QLabel(
        QStringLiteral("No live 3D preview — transforms are applied for real by Blender on export."), this));

    auto *form = new QFormLayout();

    m_scaleSpin = new QDoubleSpinBox(this);
    m_scaleSpin->setRange(0.01, 100.0);
    m_scaleSpin->setSingleStep(0.1);
    m_scaleSpin->setValue(1.0);
    m_scaleSpin->setSuffix(QStringLiteral("x"));
    form->addRow(QStringLiteral("Scale:"), m_scaleSpin);

    m_rotateXSpin = new QDoubleSpinBox(this);
    m_rotateXSpin->setRange(-360.0, 360.0);
    m_rotateXSpin->setSuffix(QStringLiteral(" deg"));
    form->addRow(QStringLiteral("Rotate X:"), m_rotateXSpin);

    m_rotateYSpin = new QDoubleSpinBox(this);
    m_rotateYSpin->setRange(-360.0, 360.0);
    m_rotateYSpin->setSuffix(QStringLiteral(" deg"));
    form->addRow(QStringLiteral("Rotate Y:"), m_rotateYSpin);

    m_rotateZSpin = new QDoubleSpinBox(this);
    m_rotateZSpin->setRange(-360.0, 360.0);
    m_rotateZSpin->setSuffix(QStringLiteral(" deg"));
    form->addRow(QStringLiteral("Rotate Z:"), m_rotateZSpin);

    m_centerCheck = new QCheckBox(QStringLiteral("Recenter origin (move bounding-box center to 0,0,0)"), this);
    form->addRow(QString(), m_centerCheck);

    m_decimateSpin = new QDoubleSpinBox(this);
    m_decimateSpin->setRange(0.01, 1.0);
    m_decimateSpin->setSingleStep(0.05);
    m_decimateSpin->setValue(1.0);
    m_decimateSpin->setToolTip(QStringLiteral("1.0 = keep all polygons; lower values reduce the mesh (decimate)."));
    form->addRow(QStringLiteral("Polygon ratio:"), m_decimateSpin);

    m_outputFormatCombo = new QComboBox(this);
    m_outputFormatCombo->addItems({"obj", "fbx", "glb", "gltf", "stl", "ply", "dae"});
    form->addRow(QStringLiteral("Output format:"), m_outputFormatCombo);

    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto *exportButton = buttons->addButton(QStringLiteral("Export"), QDialogButtonBox::AcceptRole);
    exportButton->setDefault(true);
    connect(exportButton, &QPushButton::clicked, this, &Model3DEditorDialog::exportEdit);
    layout->addWidget(buttons);
}

void Model3DEditorDialog::exportEdit() {
    const QFileInfo inputInfo(m_inputPath);
    const QString targetExt = m_outputFormatCombo->currentText();
    QString outputPath = inputInfo.absoluteDir().filePath(QStringLiteral("Edited model.%1").arg(targetExt));
    int suffix = 2;
    while (QFileInfo::exists(outputPath)) {
        outputPath = inputInfo.absoluteDir().filePath(QStringLiteral("Edited model %1.%2").arg(suffix++).arg(targetExt));
    }

    auto job = std::make_unique<ConversionJob>(m_inputPath, outputPath);
    job->setSourceFormat(inputInfo.suffix().toLower());
    job->setTargetFormat(targetExt);
    job->setEngineName(QStringLiteral("3D Model Tools"));
    job->setParameters({
        {QStringLiteral("scale"), m_scaleSpin->value()},
        {QStringLiteral("rotateX"), m_rotateXSpin->value()},
        {QStringLiteral("rotateY"), m_rotateYSpin->value()},
        {QStringLiteral("rotateZ"), m_rotateZSpin->value()},
        {QStringLiteral("center"), m_centerCheck->isChecked()},
        {QStringLiteral("decimateRatio"), m_decimateSpin->value()},
    });

    m_jobManager->addJob(std::move(job));
    m_jobManager->startQueue();
    accept();
}

} // namespace magnify::ui
