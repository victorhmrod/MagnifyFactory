#pragma once

#include <QDialog>
#include <QPair>
#include <QPixmap>
#include <QString>
#include <QVariantMap>
#include <QVector>

#include "core/FormatRegistry.h"

QT_BEGIN_NAMESPACE
class QVBoxLayout;
class QLabel;
class QPushButton;
QT_END_NAMESPACE

namespace magnify::core {
class JobManager;
}

namespace magnify::ui {

// Modal popup shown as soon as a file is added (drag-and-drop, browse, or
// the Windows "Convert with MagnifyFactory" context menu). Presents target
// formats as a grid of buttons grouped by category, plus a row of presets
// (if any exist for the category) that set both the format and a bundle of
// engine parameters at once.
//
// For Image/PDF sources with a single file, picking a format or tool does
// NOT immediately accept the dialog: it stages the pick and updates an
// inline original-vs-result preview pane, and a separate "Convert" button
// commits it. Other categories (no cheap visual preview to show) keep the
// original one-click-to-accept behavior.
class ConvertDialog : public QDialog {
    Q_OBJECT
public:
    // isSingleFile enables the Trim tool (Video/Audio) and the preview pane
    // (Image/PDF) — both need one real file path, so neither is offered for
    // a batch of files.
    // jobManager, when non-null, enables the Image category's "Edit
    // Image..." tool (single file only), which opens ImageEditorDialog
    // directly and enqueues its own job — this dialog then closes without
    // MainWindow enqueueing anything else (selectedFormat() stays empty).
    ConvertDialog(const QString &fileName, magnify::core::FormatCategory sourceCategory, QWidget *parent = nullptr,
                  bool isSingleFile = false, magnify::core::JobManager *jobManager = nullptr);

    QString selectedFormat() const { return m_selectedFormat; }
    // Empty unless a preset button was picked; MainWindow merges this into
    // the job's parameters instead of the plain-format defaults.
    QVariantMap selectedParameters() const { return m_selectedParameters; }

private:
    void addFormatSection(QVBoxLayout *layout, const QString &title, magnify::core::FormatCategory category);
    void addCustomSection(QVBoxLayout *layout, const QString &title,
                           const QVector<QPair<QString, QString>> &formats);
    void addPresetSection(QVBoxLayout *layout, magnify::core::FormatCategory category);
    void addTrimTool(QVBoxLayout *layout, const QString &filePath);
    void addRotateTool(QVBoxLayout *layout, const QString &filePath);
    void addSubtitleExtractTool(QVBoxLayout *layout);
    void addImageEditTool(QVBoxLayout *layout, const QString &filePath);
    void addDocumentEditTool(QVBoxLayout *layout, const QString &filePath);
    void addAudioEditTool(QVBoxLayout *layout, const QString &filePath);
    void addModel3DEditTool(QVBoxLayout *layout, const QString &filePath);

    // Loads m_originalPixmap for the preview pane (Image: read directly;
    // PDF: render page 1 via pdftoppm), synchronously — this only runs once,
    // at dialog construction, for a single-file Image/PDF selection.
    void loadOriginalPreview(const QString &filePath, magnify::core::FormatCategory category);
    QWidget *buildPreviewPane();
    // Stages a pick (instead of accepting immediately) and refreshes the
    // result thumbnail; the Convert button commits it to m_selected*.
    void selectPending(const QString &ext, const QVariantMap &parameters);
    void updateResultPreview();

    QString m_selectedFormat;
    QVariantMap m_selectedParameters;
    magnify::core::JobManager *m_jobManager = nullptr;

    bool m_useConfirmFlow = false;
    magnify::core::FormatCategory m_sourceCategory;
    QString m_pendingFormat;
    QVariantMap m_pendingParameters;
    QPixmap m_originalPixmap;
    QLabel *m_originalPreviewLabel = nullptr;
    QLabel *m_resultPreviewLabel = nullptr;
    QPushButton *m_convertButton = nullptr;
};

} // namespace magnify::ui
