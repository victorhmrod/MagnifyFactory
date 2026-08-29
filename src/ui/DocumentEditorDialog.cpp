#include "DocumentEditorDialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPixmap>
#include <QProcess>
#include <QPushButton>
#include <QSize>
#include <QTimer>
#include <QTransform>
#include <QUuid>
#include <QVBoxLayout>

#include "core/ConversionJob.h"
#include "core/HostProcess.h"
#include "core/JobManager.h"
#include "engines/pdf/PdfEngine.h"

using magnify::core::ConversionJob;
using magnify::core::JobManager;
using magnify::engines::pdf::PdfEngine;

namespace magnify::ui {

namespace {
constexpr int RoleSourceIndex = Qt::UserRole + 1;
constexpr int RolePageNumber = Qt::UserRole + 2;
constexpr int RoleRotation = Qt::UserRole + 3;
constexpr QSize kThumbnailSize(120, 160);

// Renders one page to a PNG via pdftoppm at a fixed low DPI (thumbnails
// only need to be recognizable, not sharp) and returns it scaled to fit
// kThumbnailSize, or a null QPixmap on failure.
QPixmap renderThumbnail(const QString &pdfPath, int pageNumber) {
    const QString prefix = QDir(magnify::core::HostProcess::sharedTempDir())
                                .filePath(QStringLiteral("magnify_docthumb_%1")
                                              .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    QProcess process;
    magnify::core::HostProcess::start(&process, QStringLiteral("pdftoppm"),
                                       {QStringLiteral("-png"), QStringLiteral("-singlefile"), QStringLiteral("-r"),
                                        QStringLiteral("50"), QStringLiteral("-f"), QString::number(pageNumber),
                                        QStringLiteral("-l"), QString::number(pageNumber), pdfPath, prefix});
    if (!process.waitForFinished(15000) || process.exitCode() != 0) {
        return {};
    }
    const QString pngPath = prefix + QStringLiteral(".png");
    QPixmap pixmap(pngPath);
    QFile::remove(pngPath);
    if (pixmap.isNull()) {
        return {};
    }
    return pixmap.scaled(kThumbnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QPixmap rotatePixmap(const QPixmap &pixmap, int degrees) {
    if (degrees == 0 || pixmap.isNull()) {
        return pixmap;
    }
    return pixmap.transformed(QTransform().rotate(degrees), Qt::SmoothTransformation);
}
} // namespace

DocumentEditorDialog::DocumentEditorDialog(const QString &filePath, JobManager *jobManager, QWidget *parent)
    : QDialog(parent), m_jobManager(jobManager) {
    setWindowTitle(QStringLiteral("Document Editor"));
    setMinimumSize(760, 560);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        QStringLiteral("Drag pages to reorder. Select one or more to delete or rotate."), this));

    m_pageGrid = new QListWidget(this);
    m_pageGrid->setViewMode(QListWidget::IconMode);
    m_pageGrid->setIconSize(kThumbnailSize);
    m_pageGrid->setGridSize(QSize(kThumbnailSize.width() + 24, kThumbnailSize.height() + 40));
    m_pageGrid->setResizeMode(QListWidget::Adjust);
    m_pageGrid->setMovement(QListWidget::Snap);
    m_pageGrid->setDragDropMode(QAbstractItemView::InternalMove);
    m_pageGrid->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_pageGrid->setSpacing(8);
    layout->addWidget(m_pageGrid, 1);

    auto *pageButtons = new QHBoxLayout();
    auto *insertButton = new QPushButton(QStringLiteral("Insert Pages From PDF..."), this);
    connect(insertButton, &QPushButton::clicked, this, &DocumentEditorDialog::insertPdf);
    pageButtons->addWidget(insertButton);
    auto *rotateLeftButton = new QPushButton(QStringLiteral("Rotate Left"), this);
    connect(rotateLeftButton, &QPushButton::clicked, this, [this]() { rotateSelectedPages(-90); });
    pageButtons->addWidget(rotateLeftButton);
    auto *rotateRightButton = new QPushButton(QStringLiteral("Rotate Right"), this);
    connect(rotateRightButton, &QPushButton::clicked, this, [this]() { rotateSelectedPages(90); });
    pageButtons->addWidget(rotateRightButton);
    auto *deleteButton = new QPushButton(QStringLiteral("Delete Selected"), this);
    connect(deleteButton, &QPushButton::clicked, this, &DocumentEditorDialog::deleteSelectedPages);
    pageButtons->addWidget(deleteButton);
    pageButtons->addStretch(1);
    layout->addLayout(pageButtons);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto *exportButton = buttons->addButton(QStringLiteral("Export"), QDialogButtonBox::AcceptRole);
    exportButton->setDefault(true);
    connect(exportButton, &QPushButton::clicked, this, &DocumentEditorDialog::exportEdit);
    layout->addWidget(buttons);

    if (!appendSourcePages(filePath)) {
        // appendSourcePages already showed the error; nothing usable to edit.
        QTimer::singleShot(0, this, &QDialog::reject);
    }
}

bool DocumentEditorDialog::appendSourcePages(const QString &path) {
    const int pageCount = PdfEngine::pageCount(path);
    if (pageCount <= 0) {
        QMessageBox::warning(this, QStringLiteral("Document Editor"),
                              QStringLiteral("Could not read \"%1\" as a PDF (is qpdf installed?).")
                                  .arg(QFileInfo(path).fileName()));
        return false;
    }

    const int sourceIndex = m_sources.size();
    m_sources << path;

    for (int page = 1; page <= pageCount; ++page) {
        const QPixmap thumbnail = renderThumbnail(path, page);
        auto *item = new QListWidgetItem(thumbnail.isNull() ? QIcon() : QIcon(thumbnail),
                                          QStringLiteral("Page %1").arg(page));
        item->setData(RoleSourceIndex, sourceIndex);
        item->setData(RolePageNumber, page);
        item->setData(RoleRotation, 0);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        m_pageGrid->addItem(item);
    }
    return true;
}

void DocumentEditorDialog::insertPdf() {
    const QString path =
        QFileDialog::getOpenFileName(this, QStringLiteral("Insert pages from PDF"), QString(), QStringLiteral("PDF files (*.pdf)"));
    if (!path.isEmpty()) {
        appendSourcePages(path);
    }
}

void DocumentEditorDialog::deleteSelectedPages() {
    const QList<QListWidgetItem *> selected = m_pageGrid->selectedItems();
    if (selected.isEmpty()) {
        return;
    }
    if (selected.size() >= m_pageGrid->count()) {
        QMessageBox::warning(this, QStringLiteral("Document Editor"),
                              QStringLiteral("The document needs at least one page."));
        return;
    }
    for (QListWidgetItem *item : selected) {
        delete m_pageGrid->takeItem(m_pageGrid->row(item));
    }
}

void DocumentEditorDialog::rotateSelectedPages(int deltaDegrees) {
    const QList<QListWidgetItem *> selected = m_pageGrid->selectedItems();
    for (QListWidgetItem *item : selected) {
        const int newRotation = ((item->data(RoleRotation).toInt() + deltaDegrees) % 360 + 360) % 360;
        item->setData(RoleRotation, newRotation);
        item->setIcon(QIcon(rotatePixmap(item->icon().pixmap(kThumbnailSize), deltaDegrees)));
    }
}

void DocumentEditorDialog::exportEdit() {
    const int pageTotal = m_pageGrid->count();
    if (pageTotal == 0) {
        QMessageBox::warning(this, QStringLiteral("Document Editor"), QStringLiteral("Nothing to export."));
        return;
    }

    QVariantList pages;
    QVariantMap rotations;
    for (int row = 0; row < pageTotal; ++row) {
        QListWidgetItem *item = m_pageGrid->item(row);
        pages << QVariantMap{{QStringLiteral("source"), item->data(RoleSourceIndex)},
                              {QStringLiteral("page"), item->data(RolePageNumber)}};
        const int rotation = item->data(RoleRotation).toInt();
        if (rotation != 0) {
            rotations.insert(QString::number(row + 1), rotation);
        }
    }

    const QFileInfo firstInfo(m_sources.first());
    QString outputPath = firstInfo.absoluteDir().filePath(QStringLiteral("Edited document.pdf"));
    int suffix = 2;
    while (QFileInfo::exists(outputPath)) {
        outputPath = firstInfo.absoluteDir().filePath(QStringLiteral("Edited document %1.pdf").arg(suffix++));
    }

    auto job = std::make_unique<ConversionJob>(m_sources.first(), outputPath);
    job->setExtraInputPaths(m_sources.mid(1));
    job->setSourceFormat(QStringLiteral("pdf"));
    job->setTargetFormat(QStringLiteral("pdf"));
    job->setEngineName(QStringLiteral("PDF Tools"));
    job->setParameters({
        {QStringLiteral("operation"), QStringLiteral("documentEdit")},
        {QStringLiteral("pages"), pages},
        {QStringLiteral("rotations"), rotations},
    });

    m_jobManager->addJob(std::move(job));
    m_jobManager->startQueue();
    accept();
}

} // namespace magnify::ui
