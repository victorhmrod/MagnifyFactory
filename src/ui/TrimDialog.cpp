#include "TrimDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QTime>
#include <QTimeEdit>
#include <QVBoxLayout>
#include <algorithm>

#include "engines/ffmpeg/FFprobe.h"

using magnify::engines::ffmpeg::FFprobe;

namespace magnify::ui {

namespace {
QTime secondsToTime(double seconds) {
    const int total = std::max(0, static_cast<int>(seconds));
    return QTime(total / 3600, (total / 60) % 60, total % 60);
}
} // namespace

TrimDialog::TrimDialog(const QString &filePath, QWidget *parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("Trim"));
    setModal(true);

    const auto probe = FFprobe::probe(filePath);
    m_durationSeconds = probe.valid ? probe.durationSeconds : 0.0;

    auto *layout = new QVBoxLayout(this);

    auto *info = new QLabel(
        QStringLiteral("Duration: %1").arg(secondsToTime(m_durationSeconds).toString(QStringLiteral("hh:mm:ss"))),
        this);
    layout->addWidget(info);

    auto *form = new QFormLayout();
    m_startEdit = new QTimeEdit(QTime(0, 0, 0), this);
    m_startEdit->setDisplayFormat(QStringLiteral("hh:mm:ss"));
    m_endEdit = new QTimeEdit(secondsToTime(m_durationSeconds), this);
    m_endEdit->setDisplayFormat(QStringLiteral("hh:mm:ss"));
    form->addRow(QStringLiteral("Start:"), m_startEdit);
    form->addRow(QStringLiteral("End:"), m_endEdit);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        if (startSeconds() >= endSeconds()) {
            QMessageBox::warning(this, QStringLiteral("Trim"), QStringLiteral("Start time must be before end time."));
            return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

double TrimDialog::startSeconds() const {
    return QTime(0, 0, 0).secsTo(m_startEdit->time());
}

double TrimDialog::endSeconds() const {
    return QTime(0, 0, 0).secsTo(m_endEdit->time());
}

} // namespace magnify::ui
