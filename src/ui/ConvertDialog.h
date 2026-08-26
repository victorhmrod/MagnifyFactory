#pragma once

#include <QDialog>
#include <QPair>
#include <QString>
#include <QVector>

#include "core/FormatRegistry.h"

QT_BEGIN_NAMESPACE
class QVBoxLayout;
QT_END_NAMESPACE

namespace magnify::ui {

// Modal popup shown as soon as a file is added (drag-and-drop, browse, or
// the Windows "Convert with MagnifyFactory" context menu). Presents target
// formats as a grid of buttons grouped by category; picking one immediately
// accepts the dialog with that extension available via selectedFormat().
class ConvertDialog : public QDialog {
    Q_OBJECT
public:
    ConvertDialog(const QString &fileName, magnify::core::FormatCategory sourceCategory, QWidget *parent = nullptr);

    QString selectedFormat() const { return m_selectedFormat; }

private:
    void addFormatSection(QVBoxLayout *layout, const QString &title, magnify::core::FormatCategory category);
    void addCustomSection(QVBoxLayout *layout, const QString &title,
                           const QVector<QPair<QString, QString>> &formats);

    QString m_selectedFormat;
};

} // namespace magnify::ui
