#pragma once

#include <QDialog>
#include <QPair>
#include <QString>
#include <QVariantMap>
#include <QVector>

#include "core/FormatRegistry.h"

QT_BEGIN_NAMESPACE
class QVBoxLayout;
QT_END_NAMESPACE

namespace magnify::ui {

// Modal popup shown as soon as a file is added (drag-and-drop, browse, or
// the Windows "Convert with MagnifyFactory" context menu). Presents target
// formats as a grid of buttons grouped by category, plus a row of presets
// (if any exist for the category) that set both the format and a bundle of
// engine parameters at once. Picking either accepts the dialog.
class ConvertDialog : public QDialog {
    Q_OBJECT
public:
    // isSingleFile enables the Trim tool (Video/Audio) — it needs one real
    // file path to probe and trim, so it's not offered for a batch of files.
    ConvertDialog(const QString &fileName, magnify::core::FormatCategory sourceCategory, QWidget *parent = nullptr,
                  bool isSingleFile = false);

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

    QString m_selectedFormat;
    QVariantMap m_selectedParameters;
};

} // namespace magnify::ui
