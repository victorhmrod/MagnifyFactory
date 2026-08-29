#include "ImageEditorDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include "CropLabel.h"
#include "core/ConversionJob.h"
#include "core/JobManager.h"

using magnify::core::ConversionJob;
using magnify::core::JobManager;

namespace magnify::ui {

namespace {
constexpr int kMaxPreviewDim = 480;

// Approximate, fast, per-pixel color adjustment for the live preview only —
// not meant to match ffmpeg's eq filter math exactly, just to look right.
// The real output always goes through eq= for real.
QImage applyColorAdjustments(const QImage &input, double brightness, double contrast, double saturation) {
    if (brightness == 0.0 && contrast == 1.0 && saturation == 1.0) {
        return input;
    }
    QImage img = input.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            QColor c(line[x]);
            float h, s, v, a;
            c.getHsvF(&h, &s, &v, &a);
            s = static_cast<float>(qBound(0.0, static_cast<double>(s) * saturation, 1.0));
            v = static_cast<float>(qBound(0.0, static_cast<double>(v) + brightness, 1.0));
            c.setHsvF(h < 0 ? 0 : h, s, v, a);
            const int r = qBound(0, static_cast<int>((c.red() - 128) * contrast + 128), 255);
            const int g = qBound(0, static_cast<int>((c.green() - 128) * contrast + 128), 255);
            const int b = qBound(0, static_cast<int>((c.blue() - 128) * contrast + 128), 255);
            line[x] = qRgba(r, g, b, c.alpha());
        }
    }
    return img;
}

// Cheap scale-down-then-up softening as a blur stand-in for the preview.
QImage applyBlurApprox(const QImage &input, double blur) {
    if (blur <= 0.0) {
        return input;
    }
    const double factor = qBound(0.05, 1.0 / (1.0 + blur * 0.5), 1.0);
    const QSize small = (input.size() * factor).expandedTo(QSize(4, 4));
    return input.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
        .scaled(input.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

void drawOverlayText(QImage &img, const QString &text, const QString &position, int fontSize) {
    if (text.isEmpty()) {
        return;
    }
    QPainter painter(&img);
    QFont font = painter.font();
    font.setPixelSize(qMax(6, fontSize));
    font.setBold(true);
    painter.setFont(font);
    const QFontMetrics fm(font);
    const QRect textRect = fm.boundingRect(text);
    constexpr int margin = 10;
    int x = margin, y = margin + fm.ascent();
    if (position == QStringLiteral("top-right")) {
        x = img.width() - textRect.width() - margin;
    } else if (position == QStringLiteral("bottom-left")) {
        y = img.height() - margin;
    } else if (position == QStringLiteral("bottom-right")) {
        x = img.width() - textRect.width() - margin;
        y = img.height() - margin;
    } else if (position == QStringLiteral("center")) {
        x = (img.width() - textRect.width()) / 2;
        y = (img.height() + fm.ascent()) / 2;
    }
    painter.setPen(Qt::black);
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            if (dx || dy) {
                painter.drawText(x + dx, y + dy, text);
            }
        }
    }
    painter.setPen(Qt::white);
    painter.drawText(x, y, text);
}
} // namespace

ImageEditorDialog::ImageEditorDialog(const QString &filePath, JobManager *jobManager, QWidget *parent)
    : QDialog(parent), m_jobManager(jobManager), m_sourcePath(filePath) {
    setWindowTitle(QStringLiteral("Image Editor"));
    setMinimumSize(760, 640);

    m_sourceImage = QImage(filePath);
    m_sourceWidth = m_sourceImage.width();
    m_sourceHeight = m_sourceImage.height();

    auto *layout = new QVBoxLayout(this);

    if (m_sourceImage.isNull()) {
        layout->addWidget(new QLabel(QStringLiteral("Could not load this image."), this));
    } else {
        layout->addWidget(new QLabel(QStringLiteral("Drag directly on the image to select a crop region:"), this));

        auto *scrollArea = new QScrollArea(this);
        scrollArea->setWidgetResizable(false);
        scrollArea->setAlignment(Qt::AlignCenter);
        m_cropLabel = new CropLabel(scrollArea);
        connect(m_cropLabel, &CropLabel::cropChanged, this, [this]() {
            const QRect crop = m_cropLabel->cropRect();
            if (crop.isValid()) {
                m_cropInfoLabel->setText(
                    QStringLiteral("Crop: %1x%2 at (%3,%4)").arg(crop.width()).arg(crop.height()).arg(crop.x()).arg(crop.y()));
                m_updatingResizeFields = true;
                m_resizeWidthSpin->setValue(crop.width());
                m_resizeHeightSpin->setValue(crop.height());
                m_updatingResizeFields = false;
            } else {
                m_cropInfoLabel->setText(QStringLiteral("No crop selected (full image will be used)"));
            }
        });
        scrollArea->setWidget(m_cropLabel);
        scrollArea->setMinimumHeight(320);
        layout->addWidget(scrollArea, 1);

        m_cropInfoLabel = new QLabel(QStringLiteral("No crop selected (full image will be used)"), this);
        layout->addWidget(m_cropInfoLabel);
        auto *clearCropButton = new QPushButton(QStringLiteral("Clear Crop"), this);
        connect(clearCropButton, &QPushButton::clicked, this, [this]() { m_cropLabel->clearCrop(); });
        layout->addWidget(clearCropButton, 0, Qt::AlignLeft);
    }

    auto *resizeRow = new QHBoxLayout();
    resizeRow->addWidget(new QLabel(QStringLiteral("Output size:"), this));
    m_resizeWidthSpin = new QSpinBox(this);
    m_resizeWidthSpin->setRange(1, 16384);
    m_resizeWidthSpin->setValue(qMax(1, m_sourceWidth));
    resizeRow->addWidget(m_resizeWidthSpin);
    resizeRow->addWidget(new QLabel(QStringLiteral("x"), this));
    m_resizeHeightSpin = new QSpinBox(this);
    m_resizeHeightSpin->setRange(1, 16384);
    m_resizeHeightSpin->setValue(qMax(1, m_sourceHeight));
    resizeRow->addWidget(m_resizeHeightSpin);
    m_keepAspectCheck = new QCheckBox(QStringLiteral("Keep aspect ratio"), this);
    m_keepAspectCheck->setChecked(true);
    resizeRow->addWidget(m_keepAspectCheck);
    resizeRow->addStretch(1);
    layout->addLayout(resizeRow);

    connect(m_resizeWidthSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int width) {
        if (m_updatingResizeFields || !m_keepAspectCheck->isChecked() || m_sourceWidth <= 0) {
            return;
        }
        m_updatingResizeFields = true;
        m_resizeHeightSpin->setValue(qMax(1, width * m_sourceHeight / m_sourceWidth));
        m_updatingResizeFields = false;
    });
    connect(m_resizeHeightSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int height) {
        if (m_updatingResizeFields || !m_keepAspectCheck->isChecked() || m_sourceHeight <= 0) {
            return;
        }
        m_updatingResizeFields = true;
        m_resizeWidthSpin->setValue(qMax(1, height * m_sourceWidth / m_sourceHeight));
        m_updatingResizeFields = false;
    });

    auto *adjustGroup = new QGroupBox(QStringLiteral("Adjustments"), this);
    auto *adjustForm = new QFormLayout(adjustGroup);
    m_brightnessSlider = new QSlider(Qt::Horizontal, adjustGroup);
    m_brightnessSlider->setRange(-100, 100);
    adjustForm->addRow(QStringLiteral("Brightness:"), m_brightnessSlider);
    m_contrastSlider = new QSlider(Qt::Horizontal, adjustGroup);
    m_contrastSlider->setRange(0, 200);
    m_contrastSlider->setValue(100);
    adjustForm->addRow(QStringLiteral("Contrast:"), m_contrastSlider);
    m_saturationSlider = new QSlider(Qt::Horizontal, adjustGroup);
    m_saturationSlider->setRange(0, 300);
    m_saturationSlider->setValue(100);
    adjustForm->addRow(QStringLiteral("Saturation:"), m_saturationSlider);
    m_blurSlider = new QSlider(Qt::Horizontal, adjustGroup);
    m_blurSlider->setRange(0, 100);
    adjustForm->addRow(QStringLiteral("Blur:"), m_blurSlider);
    layout->addWidget(adjustGroup);

    for (QSlider *slider : {m_brightnessSlider, m_contrastSlider, m_saturationSlider, m_blurSlider}) {
        connect(slider, &QSlider::valueChanged, this, &ImageEditorDialog::updatePreview);
    }

    auto *overlayGroup = new QGroupBox(QStringLiteral("Text overlay (optional)"), this);
    auto *overlayForm = new QFormLayout(overlayGroup);
    m_overlayTextEdit = new QLineEdit(overlayGroup);
    m_overlayTextEdit->setPlaceholderText(QStringLiteral("Leave blank for no overlay"));
    connect(m_overlayTextEdit, &QLineEdit::textChanged, this, &ImageEditorDialog::updatePreview);
    overlayForm->addRow(QStringLiteral("Text:"), m_overlayTextEdit);
    m_overlayPositionCombo = new QComboBox(overlayGroup);
    m_overlayPositionCombo->addItem(QStringLiteral("Bottom right"), QStringLiteral("bottom-right"));
    m_overlayPositionCombo->addItem(QStringLiteral("Bottom left"), QStringLiteral("bottom-left"));
    m_overlayPositionCombo->addItem(QStringLiteral("Top right"), QStringLiteral("top-right"));
    m_overlayPositionCombo->addItem(QStringLiteral("Top left"), QStringLiteral("top-left"));
    m_overlayPositionCombo->addItem(QStringLiteral("Center"), QStringLiteral("center"));
    connect(m_overlayPositionCombo, &QComboBox::currentIndexChanged, this, &ImageEditorDialog::updatePreview);
    overlayForm->addRow(QStringLiteral("Position:"), m_overlayPositionCombo);
    m_overlayFontSizeSpin = new QSpinBox(overlayGroup);
    m_overlayFontSizeSpin->setRange(8, 400);
    m_overlayFontSizeSpin->setValue(32);
    connect(m_overlayFontSizeSpin, qOverload<int>(&QSpinBox::valueChanged), this, &ImageEditorDialog::updatePreview);
    overlayForm->addRow(QStringLiteral("Font size:"), m_overlayFontSizeSpin);
    layout->addWidget(overlayGroup);

    auto *outputRow = new QHBoxLayout();
    outputRow->addWidget(new QLabel(QStringLiteral("Output format:"), this));
    m_outputFormatCombo = new QComboBox(this);
    m_outputFormatCombo->addItems({"png", "jpg", "webp"});
    outputRow->addWidget(m_outputFormatCombo);
    outputRow->addStretch(1);
    layout->addLayout(outputRow);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto *exportButton = buttons->addButton(QStringLiteral("Export"), QDialogButtonBox::AcceptRole);
    exportButton->setDefault(true);
    exportButton->setEnabled(!m_sourceImage.isNull());
    connect(exportButton, &QPushButton::clicked, this, &ImageEditorDialog::exportEdit);
    layout->addWidget(buttons);

    updatePreview();
}

void ImageEditorDialog::updatePreview() {
    if (m_sourceImage.isNull() || !m_cropLabel) {
        return;
    }

    QImage preview = m_sourceImage;
    if (preview.width() > kMaxPreviewDim || preview.height() > kMaxPreviewDim) {
        preview = preview.scaled(kMaxPreviewDim, kMaxPreviewDim, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    preview = applyColorAdjustments(preview, m_brightnessSlider->value() / 100.0, m_contrastSlider->value() / 100.0,
                                     m_saturationSlider->value() / 100.0);
    preview = applyBlurApprox(preview, m_blurSlider->value() / 10.0);

    const int scaledFontSize =
        m_sourceWidth > 0 ? m_overlayFontSizeSpin->value() * preview.width() / m_sourceWidth : m_overlayFontSizeSpin->value();
    drawOverlayText(preview, m_overlayTextEdit->text(), m_overlayPositionCombo->currentData().toString(),
                     scaledFontSize);

    m_cropLabel->setPreviewImage(preview, QSize(m_sourceWidth, m_sourceHeight));
}

void ImageEditorDialog::exportEdit() {
    if (m_sourceImage.isNull()) {
        return;
    }

    const QFileInfo info(m_sourcePath);
    const QString targetExt = m_outputFormatCombo->currentText();
    QString outputPath = info.absoluteDir().filePath(info.completeBaseName() + QStringLiteral(" (edited).") + targetExt);
    int suffix = 2;
    while (QFileInfo::exists(outputPath)) {
        outputPath = info.absoluteDir().filePath(
            QStringLiteral("%1 (edited %2).%3").arg(info.completeBaseName()).arg(suffix++).arg(targetExt));
    }

    QVariantMap params;
    params[QStringLiteral("operation")] = QStringLiteral("imageEdit");

    const QRect crop = m_cropLabel->cropRect();
    const int baseWidth = crop.isValid() ? crop.width() : m_sourceWidth;
    const int baseHeight = crop.isValid() ? crop.height() : m_sourceHeight;
    if (crop.isValid()) {
        params[QStringLiteral("crop")] = QVariantList{crop.x(), crop.y(), crop.width(), crop.height()};
    }
    if (m_resizeWidthSpin->value() != baseWidth || m_resizeHeightSpin->value() != baseHeight) {
        params[QStringLiteral("resizeWidth")] = m_resizeWidthSpin->value();
        params[QStringLiteral("resizeHeight")] = m_resizeHeightSpin->value();
    }

    params[QStringLiteral("brightness")] = m_brightnessSlider->value() / 100.0;
    params[QStringLiteral("contrast")] = m_contrastSlider->value() / 100.0;
    params[QStringLiteral("saturation")] = m_saturationSlider->value() / 100.0;
    params[QStringLiteral("blur")] = m_blurSlider->value() / 10.0;
    params[QStringLiteral("overlayText")] = m_overlayTextEdit->text();
    params[QStringLiteral("overlayPosition")] = m_overlayPositionCombo->currentData().toString();
    params[QStringLiteral("overlayFontSize")] = m_overlayFontSizeSpin->value();

    auto job = std::make_unique<ConversionJob>(m_sourcePath, outputPath);
    job->setSourceFormat(info.suffix().toLower());
    job->setTargetFormat(targetExt);
    job->setEngineName(QStringLiteral("FFmpeg"));
    job->setParameters(params);

    m_jobManager->addJob(std::move(job));
    m_jobManager->startQueue();
    accept();
}

} // namespace magnify::ui
