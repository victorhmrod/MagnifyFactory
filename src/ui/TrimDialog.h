#pragma once

#include <QDialog>
#include <QString>

QT_BEGIN_NAMESPACE
class QTimeEdit;
QT_END_NAMESPACE

namespace magnify::ui {

// Small modal for picking a start/end time to cut a video or audio file to.
// Probes the real file (via FFprobe) to show its duration and bound the
// range pickers; startSeconds()/endSeconds() are only meaningful after the
// dialog is accepted.
class TrimDialog : public QDialog {
    Q_OBJECT
public:
    explicit TrimDialog(const QString &filePath, QWidget *parent = nullptr);

    double startSeconds() const;
    double endSeconds() const;

private:
    QTimeEdit *m_startEdit = nullptr;
    QTimeEdit *m_endEdit = nullptr;
    double m_durationSeconds = 0.0;
};

} // namespace magnify::ui
