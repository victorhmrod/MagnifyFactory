#include "ConvertDialog.h"

#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using magnify::core::FormatCategory;
using magnify::core::FormatRegistry;

namespace magnify::ui {

ConvertDialog::ConvertDialog(const QString &fileName, FormatCategory sourceCategory, QWidget *parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("Convert"));
    setModal(true);
    setMinimumWidth(420);
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

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(4);

    auto *title = new QLabel(QStringLiteral("Convert \"%1\" to:").arg(QFileInfo(fileName).fileName()), this);
    title->setObjectName(QStringLiteral("title"));
    title->setWordWrap(true);
    layout->addWidget(title);

    // Offer the source's own category first, plus sensible cross-category
    // targets (e.g. a video can be converted straight to an audio format).
    switch (sourceCategory) {
        case FormatCategory::Video:
            addFormatSection(layout, QStringLiteral("VIDEO"), FormatCategory::Video);
            addFormatSection(layout, QStringLiteral("EXTRACT AUDIO"), FormatCategory::Audio);
            break;
        case FormatCategory::Audio:
            addFormatSection(layout, QStringLiteral("AUDIO"), FormatCategory::Audio);
            break;
        case FormatCategory::Image:
            addFormatSection(layout, QStringLiteral("IMAGE"), FormatCategory::Image);
            break;
        case FormatCategory::Pdf:
            addFormatSection(layout, QStringLiteral("PDF"), FormatCategory::Pdf);
            break;
        default:
            addFormatSection(layout, QStringLiteral("VIDEO"), FormatCategory::Video);
            addFormatSection(layout, QStringLiteral("AUDIO"), FormatCategory::Audio);
            addFormatSection(layout, QStringLiteral("IMAGE"), FormatCategory::Image);
            break;
    }
}

void ConvertDialog::addFormatSection(QVBoxLayout *layout, const QString &title, FormatCategory category) {
    const auto formats = FormatRegistry::instance().formatsInCategory(category);
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
    for (const auto &descriptor : formats) {
        if (!descriptor.supportsOutput) {
            continue;
        }
        auto *button = new QPushButton(descriptor.name, this);
        button->setProperty("class", QStringLiteral("formatButton"));
        button->setCursor(Qt::PointingHandCursor);
        connect(button, &QPushButton::clicked, this, [this, ext = descriptor.extension]() {
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

} // namespace magnify::ui
