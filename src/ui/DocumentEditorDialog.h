#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QListWidget;
class QListWidgetItem;
QT_END_NAMESPACE

namespace magnify::core {
class JobManager;
}

namespace magnify::ui {

// PDF page editor: opens a PDF as a grid of real page-render thumbnails
// (pdftoppm), lets pages be reordered by drag, deleted, rotated in 90°
// steps, and lets pages from other PDFs be inserted anywhere in the
// sequence. Export re-derives the whole document for real via
// PdfEngine::documentEditPdf (a qpdf --pages pass, then a qpdf --rotate
// pass) — the grid is a real render of each source page, so unlike the
// Video/Image editors there is no separate "approximate preview vs real
// export" gap for content, only for rotation (thumbnails are rotated
// in-process for immediate feedback; export re-renders nothing, it just
// carries the rotation into the PDF's page metadata via qpdf).
class DocumentEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit DocumentEditorDialog(const QString &filePath, magnify::core::JobManager *jobManager,
                                   QWidget *parent = nullptr);

private:
    // Appends every page of `path` (a newly opened or inserted PDF) as new
    // items at the end of the grid, tracking it as a new entry in
    // m_sources. Returns false (and shows an error) if it isn't a readable
    // PDF via PdfEngine::pageCount.
    bool appendSourcePages(const QString &path);
    void insertPdf();
    void deleteSelectedPages();
    void rotateSelectedPages(int deltaDegrees);
    void exportEdit();

    magnify::core::JobManager *m_jobManager = nullptr;
    QListWidget *m_pageGrid = nullptr;
    QStringList m_sources;
};

} // namespace magnify::ui
