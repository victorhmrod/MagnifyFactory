#include "AudioEditorDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QTableWidget>
#include <QTime>
#include <QVBoxLayout>

#include "core/ConversionJob.h"
#include "core/JobManager.h"
#include "engines/ffmpeg/FFprobe.h"

using magnify::core::ConversionJob;
using magnify::core::JobManager;
using magnify::engines::ffmpeg::FFprobe;

namespace magnify::ui {

namespace {
constexpr int ColumnFile = 0;
constexpr int ColumnDuration = 1;
constexpr int ColumnStart = 2;
constexpr int ColumnEnd = 3;
constexpr int RolePath = Qt::UserRole + 1;
constexpr int RoleDurationSeconds = Qt::UserRole + 2;

QString formatSeconds(double seconds) {
    const int total = qMax(0, static_cast<int>(seconds));
    return QTime(0, 0, 0).addSecs(total).toString(total >= 3600 ? QStringLiteral("hh:mm:ss")
                                                                  : QStringLiteral("mm:ss"));
}

// Parses "mm:ss" or "hh:mm:ss"; empty/invalid input means "unset".
bool parseTimeField(const QString &text, double *outSeconds) {
    if (text.trimmed().isEmpty()) {
        return false;
    }
    const QStringList parts = text.trimmed().split(':');
    int h = 0, m = 0;
    double s = 0.0;
    bool ok = true;
    if (parts.size() == 3) {
        h = parts[0].toInt(&ok);
        m = ok ? parts[1].toInt(&ok) : 0;
        s = ok ? parts[2].toDouble(&ok) : 0.0;
    } else if (parts.size() == 2) {
        m = parts[0].toInt(&ok);
        s = ok ? parts[1].toDouble(&ok) : 0.0;
    } else {
        s = parts[0].toDouble(&ok);
    }
    if (!ok) {
        return false;
    }
    *outSeconds = h * 3600.0 + m * 60.0 + s;
    return true;
}
} // namespace

AudioEditorDialog::AudioEditorDialog(JobManager *jobManager, QWidget *parent)
    : QDialog(parent), m_jobManager(jobManager) {
    setWindowTitle(QStringLiteral("Audio Editor"));
    setMinimumSize(680, 520);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(QStringLiteral("Clips (in order) — each is trimmed, then all are joined into one:"), this));

    m_clipsTable = new QTableWidget(0, 4, this);
    m_clipsTable->setHorizontalHeaderLabels({"File", "Duration", "Start (mm:ss)", "End (mm:ss, blank = clip end)"});
    m_clipsTable->horizontalHeader()->setSectionResizeMode(ColumnFile, QHeaderView::Stretch);
    m_clipsTable->verticalHeader()->setVisible(false);
    m_clipsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_clipsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_clipsTable, 1);

    auto *clipButtons = new QHBoxLayout();
    auto *addButton = new QPushButton(QStringLiteral("Add Clips..."), this);
    connect(addButton, &QPushButton::clicked, this, [this]() {
        const QStringList paths = QFileDialog::getOpenFileNames(this, QStringLiteral("Add audio clips"));
        addClips(paths);
    });
    clipButtons->addWidget(addButton);
    auto *removeButton = new QPushButton(QStringLiteral("Remove Selected"), this);
    connect(removeButton, &QPushButton::clicked, this, &AudioEditorDialog::removeSelectedClip);
    clipButtons->addWidget(removeButton);
    auto *upButton = new QPushButton(QStringLiteral("Move Up"), this);
    connect(upButton, &QPushButton::clicked, this, [this]() { moveSelectedClip(-1); });
    clipButtons->addWidget(upButton);
    auto *downButton = new QPushButton(QStringLiteral("Move Down"), this);
    connect(downButton, &QPushButton::clicked, this, [this]() { moveSelectedClip(1); });
    clipButtons->addWidget(downButton);
    clipButtons->addStretch(1);
    layout->addLayout(clipButtons);

    auto *adjustGroup = new QGroupBox(QStringLiteral("Adjustments (applied to the whole joined audio)"), this);
    auto *adjustForm = new QFormLayout(adjustGroup);

    m_volumeSlider = new QSlider(Qt::Horizontal, adjustGroup);
    m_volumeSlider->setRange(-30, 30);
    m_volumeSlider->setValue(0);
    adjustForm->addRow(QStringLiteral("Volume (dB):"), m_volumeSlider);

    m_speedSpin = new QDoubleSpinBox(adjustGroup);
    m_speedSpin->setRange(0.5, 2.0);
    m_speedSpin->setSingleStep(0.1);
    m_speedSpin->setValue(1.0);
    m_speedSpin->setSuffix(QStringLiteral("x"));
    adjustForm->addRow(QStringLiteral("Speed:"), m_speedSpin);

    m_fadeInSpin = new QDoubleSpinBox(adjustGroup);
    m_fadeInSpin->setRange(0.0, 30.0);
    m_fadeInSpin->setSingleStep(0.5);
    m_fadeInSpin->setValue(0.0);
    m_fadeInSpin->setSuffix(QStringLiteral(" s"));
    adjustForm->addRow(QStringLiteral("Fade in:"), m_fadeInSpin);

    m_fadeOutSpin = new QDoubleSpinBox(adjustGroup);
    m_fadeOutSpin->setRange(0.0, 30.0);
    m_fadeOutSpin->setSingleStep(0.5);
    m_fadeOutSpin->setValue(0.0);
    m_fadeOutSpin->setSuffix(QStringLiteral(" s"));
    adjustForm->addRow(QStringLiteral("Fade out:"), m_fadeOutSpin);

    m_normalizeCheck = new QCheckBox(QStringLiteral("Normalize loudness"), adjustGroup);
    adjustForm->addRow(QString(), m_normalizeCheck);

    layout->addWidget(adjustGroup);

    auto *outputRow = new QHBoxLayout();
    outputRow->addWidget(new QLabel(QStringLiteral("Output format:"), this));
    m_outputFormatCombo = new QComboBox(this);
    m_outputFormatCombo->addItems({"mp3", "wav", "flac", "aac", "ogg"});
    outputRow->addWidget(m_outputFormatCombo);
    outputRow->addStretch(1);
    layout->addLayout(outputRow);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto *exportButton = buttons->addButton(QStringLiteral("Export"), QDialogButtonBox::AcceptRole);
    exportButton->setDefault(true);
    connect(exportButton, &QPushButton::clicked, this, &AudioEditorDialog::exportEdit);
    layout->addWidget(buttons);
}

void AudioEditorDialog::addClips(const QStringList &paths) {
    for (const QString &path : paths) {
        if (!path.isEmpty()) {
            addClipRow(path);
        }
    }
}

void AudioEditorDialog::addClipRow(const QString &path) {
    const int row = m_clipsTable->rowCount();
    m_clipsTable->insertRow(row);

    auto *fileItem = new QTableWidgetItem(QFileInfo(path).fileName());
    fileItem->setData(RolePath, path);
    fileItem->setFlags(fileItem->flags() & ~Qt::ItemIsEditable);
    m_clipsTable->setItem(row, ColumnFile, fileItem);

    const auto probe = FFprobe::probe(path);
    const double duration = probe.valid ? probe.durationSeconds : 0.0;
    auto *durationItem = new QTableWidgetItem(formatSeconds(duration));
    durationItem->setData(RoleDurationSeconds, duration);
    durationItem->setFlags(durationItem->flags() & ~Qt::ItemIsEditable);
    m_clipsTable->setItem(row, ColumnDuration, durationItem);

    m_clipsTable->setItem(row, ColumnStart, new QTableWidgetItem(QStringLiteral("0:00")));
    m_clipsTable->setItem(row, ColumnEnd, new QTableWidgetItem(QString()));
}

void AudioEditorDialog::removeSelectedClip() {
    const int row = m_clipsTable->currentRow();
    if (row >= 0) {
        m_clipsTable->removeRow(row);
    }
}

void AudioEditorDialog::moveSelectedClip(int direction) {
    const int row = m_clipsTable->currentRow();
    const int target = row + direction;
    if (row < 0 || target < 0 || target >= m_clipsTable->rowCount()) {
        return;
    }
    for (int col = 0; col < m_clipsTable->columnCount(); ++col) {
        QTableWidgetItem *a = m_clipsTable->takeItem(row, col);
        QTableWidgetItem *b = m_clipsTable->takeItem(target, col);
        m_clipsTable->setItem(row, col, b);
        m_clipsTable->setItem(target, col, a);
    }
    m_clipsTable->setCurrentCell(target, 0);
}

void AudioEditorDialog::exportEdit() {
    const int clipCount = m_clipsTable->rowCount();
    if (clipCount == 0) {
        QMessageBox::warning(this, QStringLiteral("Audio Editor"), QStringLiteral("Add at least one clip first."));
        return;
    }

    QStringList paths;
    QVariantList clipTrims;
    for (int row = 0; row < clipCount; ++row) {
        const QString path = m_clipsTable->item(row, ColumnFile)->data(RolePath).toString();
        paths << path;

        double start = 0.0;
        parseTimeField(m_clipsTable->item(row, ColumnStart)->text(), &start);
        double end = 0.0;
        const bool hasEnd = parseTimeField(m_clipsTable->item(row, ColumnEnd)->text(), &end);
        if (hasEnd && end <= start) {
            QMessageBox::warning(this, QStringLiteral("Audio Editor"),
                                  QStringLiteral("Clip %1: end time must be after start time.").arg(row + 1));
            return;
        }
        clipTrims << QVariantList{start, hasEnd ? end : -1.0};
    }

    const QFileInfo firstInfo(paths.first());
    const QString targetExt = m_outputFormatCombo->currentText();
    QString outputPath = firstInfo.absoluteDir().filePath(QStringLiteral("Edited audio.%1").arg(targetExt));
    int suffix = 2;
    while (QFileInfo::exists(outputPath)) {
        outputPath = firstInfo.absoluteDir().filePath(QStringLiteral("Edited audio %1.%2").arg(suffix++).arg(targetExt));
    }

    auto job = std::make_unique<ConversionJob>(paths.first(), outputPath);
    job->setExtraInputPaths(paths.mid(1));
    job->setSourceFormat(firstInfo.suffix().toLower());
    job->setTargetFormat(targetExt);
    job->setEngineName(QStringLiteral("FFmpeg"));
    job->setParameters({
        {QStringLiteral("operation"), QStringLiteral("audioEdit")},
        {QStringLiteral("clipTrims"), clipTrims},
        {QStringLiteral("volumeDb"), m_volumeSlider->value()},
        {QStringLiteral("speed"), m_speedSpin->value()},
        {QStringLiteral("fadeInSeconds"), m_fadeInSpin->value()},
        {QStringLiteral("fadeOutSeconds"), m_fadeOutSpin->value()},
        {QStringLiteral("normalize"), m_normalizeCheck->isChecked()},
    });

    m_jobManager->addJob(std::move(job));
    m_jobManager->startQueue();
    accept();
}

} // namespace magnify::ui
