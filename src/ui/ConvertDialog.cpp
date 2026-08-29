#include "ConvertDialog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QTransform>
#include <QUuid>
#include <QVBoxLayout>

#include "DocumentEditorDialog.h"
#include "ImageEditorDialog.h"
#include "TrimDialog.h"
#include "core/JobManager.h"
#include "presets/PresetRegistry.h"

using magnify::core::FormatCategory;
using magnify::core::FormatRegistry;
using magnify::core::JobManager;
using magnify::presets::Preset;
using magnify::presets::PresetRegistry;

namespace magnify::ui {

namespace {
constexpr int kPreviewSide = 160;

QLabel *makePreviewSlot(QWidget *parent) {
    auto *label = new QLabel(parent);
    label->setFixedSize(kPreviewSide, kPreviewSide);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setStyleSheet(
        QStringLiteral("background-color: #12141a; border: 1px solid #2a2f37; border-radius: 4px; color: #8b949e;"));
    return label;
}
} // namespace

ConvertDialog::ConvertDialog(const QString &fileName, FormatCategory sourceCategory, QWidget *parent,
                              bool isSingleFile, JobManager *jobManager)
    : QDialog(parent), m_sourceCategory(sourceCategory), m_jobManager(jobManager) {
    setWindowTitle(QStringLiteral("Convert"));
    setModal(true);
    setMinimumWidth(460);
    setStyleSheet(R"(
        QDialog { background-color: #1e2126; color: #c9d1d9; }
        QLabel#title { font-size: 15px; font-weight: 600; color: #ffffff; }
        QLabel#section { color: #8b949e; font-size: 11px; font-weight: 600; margin-top: 6px; }
        QPushButton[class="formatButton"] {
            background-color: #2d333b; border: 1px solid #3a4048; border-radius: 6px;
            padding: 10px; font-weight: 600; min-width: 64px;
        }
        QPushButton[class="formatButton"]:hover { background-color: #2f6feb; border-color: #2f6feb; color: white; }
    )");

    m_useConfirmFlow =
        isSingleFile && (sourceCategory == FormatCategory::Image || sourceCategory == FormatCategory::Pdf);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(4);

    auto *title = new QLabel(QStringLiteral("Convert \"%1\" to:").arg(QFileInfo(fileName).fileName()), this);
    title->setObjectName(QStringLiteral("title"));
    title->setWordWrap(true);
    layout->addWidget(title);

    if (m_useConfirmFlow) {
        loadOriginalPreview(fileName, sourceCategory);
        layout->addSpacing(8);
        layout->addWidget(buildPreviewPane());
        layout->addSpacing(4);
    }

    // Offer the source's own category first, plus sensible cross-category
    // targets (e.g. a video can be converted straight to an audio format).
    switch (sourceCategory) {
        case FormatCategory::Video:
            if (isSingleFile) {
                addTrimTool(layout, fileName);
            }
            addPresetSection(layout, FormatCategory::Video);
            addFormatSection(layout, QStringLiteral("VIDEO"), FormatCategory::Video);
            addFormatSection(layout, QStringLiteral("EXTRACT AUDIO"), FormatCategory::Audio);
            addSubtitleExtractTool(layout);
            break;
        case FormatCategory::Audio:
            if (isSingleFile) {
                addTrimTool(layout, fileName);
            }
            addPresetSection(layout, FormatCategory::Audio);
            addFormatSection(layout, QStringLiteral("AUDIO"), FormatCategory::Audio);
            break;
        case FormatCategory::Image:
            if (isSingleFile) {
                addRotateTool(layout, fileName);
                if (m_jobManager) {
                    addImageEditTool(layout, fileName);
                }
            }
            addPresetSection(layout, FormatCategory::Image);
            addFormatSection(layout, QStringLiteral("IMAGE"), FormatCategory::Image);
            addCustomSection(layout, QStringLiteral("DOCUMENT"), {{QStringLiteral("PDF"), QStringLiteral("pdf")}});
            break;
        case FormatCategory::Pdf:
            if (isSingleFile && m_jobManager) {
                addDocumentEditTool(layout, fileName);
            }
            // pdftoppm (the renderer behind PDF -> image) only supports these
            // two output formats; the general Image category list includes
            // WebP/AVIF/etc. which it cannot produce.
            addCustomSection(layout, QStringLiteral("IMAGE (page 1)"),
                              {{QStringLiteral("PNG"), QStringLiteral("png")},
                               {QStringLiteral("JPEG"), QStringLiteral("jpg")}});
            // Same extension in and out — PdfEngine detects this pdf->pdf
            // case and runs a qpdf compression pass instead of rendering.
            addCustomSection(layout, QStringLiteral("TOOLS"),
                              {{QStringLiteral("Compress"), QStringLiteral("pdf")}});
            addPresetSection(layout, FormatCategory::Pdf);
            break;
        case FormatCategory::Document:
            // Documents mostly convert to PDF, but LibreOffice's headless
            // --convert-to also handles cross-format document conversion
            // (e.g. docx -> odt), so offer the full Document category too.
            addCustomSection(layout, QStringLiteral("PDF"), {{QStringLiteral("PDF"), QStringLiteral("pdf")}});
            addFormatSection(layout, QStringLiteral("DOCUMENT"), FormatCategory::Document);
            break;
        default:
            addFormatSection(layout, QStringLiteral("VIDEO"), FormatCategory::Video);
            addFormatSection(layout, QStringLiteral("AUDIO"), FormatCategory::Audio);
            addFormatSection(layout, QStringLiteral("IMAGE"), FormatCategory::Image);
            break;
    }

    if (m_useConfirmFlow) {
        auto *buttonRow = new QHBoxLayout();
        buttonRow->addStretch(1);
        auto *cancelButton = new QPushButton(QStringLiteral("Cancel"), this);
        connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
        m_convertButton = new QPushButton(QStringLiteral("Convert"), this);
        m_convertButton->setEnabled(false);
        m_convertButton->setDefault(true);
        connect(m_convertButton, &QPushButton::clicked, this, [this]() {
            m_selectedFormat = m_pendingFormat;
            m_selectedParameters = m_pendingParameters;
            accept();
        });
        buttonRow->addWidget(cancelButton);
        buttonRow->addWidget(m_convertButton);
        layout->addSpacing(8);
        layout->addLayout(buttonRow);
    }
}

void ConvertDialog::loadOriginalPreview(const QString &filePath, FormatCategory category) {
    if (category == FormatCategory::Image) {
        QImageReader reader(filePath);
        reader.setAutoTransform(true);
        const QImage image = reader.read();
        if (!image.isNull()) {
            m_originalPixmap = QPixmap::fromImage(image);
        }
        return;
    }

    if (category == FormatCategory::Pdf) {
        // Render page 1 at a modest DPI for the preview thumbnail, via the
        // same pdftoppm tool PdfEngine itself uses for the real conversion.
        const QString tempPrefix =
            QDir::temp().filePath(QStringLiteral("magnify_preview_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
        QProcess proc;
        proc.start(QStringLiteral("pdftoppm"), {QStringLiteral("-png"), QStringLiteral("-singlefile"),
                                                 QStringLiteral("-r"), QStringLiteral("80"), QStringLiteral("-f"),
                                                 QStringLiteral("1"), QStringLiteral("-l"), QStringLiteral("1"),
                                                 filePath, tempPrefix});
        proc.waitForFinished(5000);
        const QString producedPath = tempPrefix + QStringLiteral(".png");
        if (QFileInfo::exists(producedPath)) {
            m_originalPixmap = QPixmap(producedPath);
            QFile::remove(producedPath);
        }
    }
}

QWidget *ConvertDialog::buildPreviewPane() {
    auto *frame = new QWidget(this);
    auto *previewLayout = new QHBoxLayout(frame);
    previewLayout->setContentsMargins(0, 0, 0, 0);

    m_originalPreviewLabel = makePreviewSlot(this);
    if (!m_originalPixmap.isNull()) {
        m_originalPreviewLabel->setPixmap(
            m_originalPixmap.scaled(kPreviewSide, kPreviewSide, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        m_originalPreviewLabel->setText(QStringLiteral("No preview"));
    }

    auto *arrowLabel = new QLabel(QStringLiteral("→"), this);
    QFont arrowFont = arrowLabel->font();
    arrowFont.setPointSize(20);
    arrowLabel->setFont(arrowFont);

    m_resultPreviewLabel = makePreviewSlot(this);
    m_resultPreviewLabel->setText(QStringLiteral("Pick a format"));

    previewLayout->addStretch(1);
    previewLayout->addWidget(m_originalPreviewLabel);
    previewLayout->addWidget(arrowLabel);
    previewLayout->addWidget(m_resultPreviewLabel);
    previewLayout->addStretch(1);
    return frame;
}

void ConvertDialog::selectPending(const QString &ext, const QVariantMap &parameters) {
    m_pendingFormat = ext;
    m_pendingParameters = parameters;
    updateResultPreview();
    if (m_convertButton) {
        m_convertButton->setEnabled(true);
    }
}

void ConvertDialog::updateResultPreview() {
    if (!m_resultPreviewLabel) {
        return;
    }

    QPixmap result = m_originalPixmap;
    if (m_pendingParameters.contains(QStringLiteral("rotate")) && !result.isNull()) {
        QTransform transform;
        transform.rotate(m_pendingParameters.value(QStringLiteral("rotate")).toInt());
        result = result.transformed(transform, Qt::SmoothTransformation);
    }

    if (result.isNull()) {
        m_resultPreviewLabel->setPixmap(QPixmap());
        m_resultPreviewLabel->setText(QStringLiteral("No preview available"));
    } else {
        m_resultPreviewLabel->setText(QString());
        m_resultPreviewLabel->setPixmap(
            result.scaled(kPreviewSide, kPreviewSide, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void ConvertDialog::addFormatSection(QVBoxLayout *layout, const QString &title, FormatCategory category) {
    QVector<QPair<QString, QString>> formats;
    for (const auto &descriptor : FormatRegistry::instance().formatsInCategory(category)) {
        if (descriptor.supportsOutput) {
            formats.append({descriptor.name, descriptor.extension});
        }
    }
    addCustomSection(layout, title, formats);
}

void ConvertDialog::addPresetSection(QVBoxLayout *layout, FormatCategory category) {
    const QVector<Preset> presets = PresetRegistry::instance().presetsForCategory(category);
    if (presets.isEmpty()) {
        return;
    }

    auto *sectionLabel = new QLabel(QStringLiteral("PRESETS"), this);
    sectionLabel->setObjectName(QStringLiteral("section"));
    layout->addWidget(sectionLabel);

    auto *grid = new QGridLayout();
    grid->setSpacing(8);
    int col = 0;
    int row = 0;
    constexpr int columns = 3;
    for (const Preset &preset : presets) {
        auto *button = new QPushButton(preset.name, this);
        button->setProperty("class", QStringLiteral("formatButton"));
        button->setCursor(Qt::PointingHandCursor);
        connect(button, &QPushButton::clicked, this, [this, preset]() {
            if (m_useConfirmFlow) {
                selectPending(preset.targetFormat, preset.parameters);
                return;
            }
            m_selectedFormat = preset.targetFormat;
            m_selectedParameters = preset.parameters;
            accept();
        });
        grid->addWidget(button, row, col);
        if (++col >= columns) {
            col = 0;
            ++row;
        }
    }
    layout->addLayout(grid);
}

void ConvertDialog::addCustomSection(QVBoxLayout *layout, const QString &title,
                                      const QVector<QPair<QString, QString>> &formats) {
    if (formats.isEmpty()) {
        return;
    }

    auto *sectionLabel = new QLabel(title, this);
    sectionLabel->setObjectName(QStringLiteral("section"));
    layout->addWidget(sectionLabel);

    auto *grid = new QGridLayout();
    grid->setSpacing(8);
    int col = 0;
    int row = 0;
    constexpr int columns = 5;
    for (const auto &[name, ext] : formats) {
        auto *button = new QPushButton(name, this);
        button->setProperty("class", QStringLiteral("formatButton"));
        button->setCursor(Qt::PointingHandCursor);
        connect(button, &QPushButton::clicked, this, [this, ext]() {
            if (m_useConfirmFlow) {
                selectPending(ext, {});
                return;
            }
            m_selectedFormat = ext;
            accept();
        });
        grid->addWidget(button, row, col);
        if (++col >= columns) {
            col = 0;
            ++row;
        }
    }
    layout->addLayout(grid);
}

void ConvertDialog::addTrimTool(QVBoxLayout *layout, const QString &filePath) {
    auto *sectionLabel = new QLabel(QStringLiteral("TOOLS"), this);
    sectionLabel->setObjectName(QStringLiteral("section"));
    layout->addWidget(sectionLabel);

    auto *button = new QPushButton(QStringLiteral("Trim..."), this);
    button->setProperty("class", QStringLiteral("formatButton"));
    button->setCursor(Qt::PointingHandCursor);
    connect(button, &QPushButton::clicked, this, [this, filePath]() {
        TrimDialog trimDialog(filePath, this);
        if (trimDialog.exec() != QDialog::Accepted) {
            return;
        }
        m_selectedFormat = QFileInfo(filePath).suffix().toLower();
        m_selectedParameters = {{QStringLiteral("trimStart"), trimDialog.startSeconds()},
                                 {QStringLiteral("trimEnd"), trimDialog.endSeconds()}};
        accept();
    });

    auto *row = new QHBoxLayout();
    row->addWidget(button);
    row->addStretch(1);
    layout->addLayout(row);
}

void ConvertDialog::addRotateTool(QVBoxLayout *layout, const QString &filePath) {
    auto *sectionLabel = new QLabel(QStringLiteral("TOOLS"), this);
    sectionLabel->setObjectName(QStringLiteral("section"));
    layout->addWidget(sectionLabel);

    const QString sourceExt = QFileInfo(filePath).suffix().toLower();

    auto *ccwButton = new QPushButton(QStringLiteral("⟲ Rotate CCW"), this);
    auto *cwButton = new QPushButton(QStringLiteral("⟳ Rotate CW"), this);
    ccwButton->setProperty("class", QStringLiteral("formatButton"));
    cwButton->setProperty("class", QStringLiteral("formatButton"));
    ccwButton->setCursor(Qt::PointingHandCursor);
    cwButton->setCursor(Qt::PointingHandCursor);

    // Each click reads the rotation staged by the previous one (0 if none
    // yet) and advances it by 90°, so repeated clicks cycle 0->90->180->270.
    connect(ccwButton, &QPushButton::clicked, this, [this, sourceExt]() {
        const int current = m_pendingParameters.value(QStringLiteral("rotate"), 0).toInt();
        selectPending(sourceExt, {{QStringLiteral("rotate"), (current + 270) % 360}});
    });
    connect(cwButton, &QPushButton::clicked, this, [this, sourceExt]() {
        const int current = m_pendingParameters.value(QStringLiteral("rotate"), 0).toInt();
        selectPending(sourceExt, {{QStringLiteral("rotate"), (current + 90) % 360}});
    });

    auto *row = new QHBoxLayout();
    row->addWidget(ccwButton);
    row->addWidget(cwButton);
    row->addStretch(1);
    layout->addLayout(row);
}

void ConvertDialog::addSubtitleExtractTool(QVBoxLayout *layout) {
    auto *sectionLabel = new QLabel(QStringLiteral("TOOLS"), this);
    sectionLabel->setObjectName(QStringLiteral("section"));
    layout->addWidget(sectionLabel);

    auto *button = new QPushButton(QStringLiteral("Extract Subtitles (.srt)"), this);
    button->setProperty("class", QStringLiteral("formatButton"));
    button->setCursor(Qt::PointingHandCursor);
    connect(button, &QPushButton::clicked, this, [this]() {
        m_selectedFormat = QStringLiteral("srt");
        m_selectedParameters = {{QStringLiteral("operation"), QStringLiteral("extractSubtitles")}};
        accept();
    });

    auto *row = new QHBoxLayout();
    row->addWidget(button);
    row->addStretch(1);
    layout->addLayout(row);
}

void ConvertDialog::addImageEditTool(QVBoxLayout *layout, const QString &filePath) {
    auto *button = new QPushButton(QStringLiteral("Edit Image..."), this);
    button->setProperty("class", QStringLiteral("formatButton"));
    button->setCursor(Qt::PointingHandCursor);
    connect(button, &QPushButton::clicked, this, [this, filePath]() {
        ImageEditorDialog editor(filePath, m_jobManager, this);
        editor.exec();
        // The editor enqueues its own job (or the user cancelled) — either
        // way MainWindow shouldn't also enqueue from this dialog's own
        // selectedFormat(), which stays empty.
        reject();
    });

    auto *row = new QHBoxLayout();
    row->addWidget(button);
    row->addStretch(1);
    layout->addLayout(row);
}

void ConvertDialog::addDocumentEditTool(QVBoxLayout *layout, const QString &filePath) {
    auto *button = new QPushButton(QStringLiteral("Edit Document..."), this);
    button->setProperty("class", QStringLiteral("formatButton"));
    button->setCursor(Qt::PointingHandCursor);
    connect(button, &QPushButton::clicked, this, [this, filePath]() {
        DocumentEditorDialog editor(filePath, m_jobManager, this);
        editor.exec();
        // Same reasoning as addImageEditTool(): the editor enqueues its own
        // job (or the user cancelled), so this dialog shouldn't also
        // enqueue from selectedFormat(), which stays empty.
        reject();
    });

    auto *row = new QHBoxLayout();
    row->addWidget(button);
    row->addStretch(1);
    layout->addLayout(row);
}

} // namespace magnify::ui
