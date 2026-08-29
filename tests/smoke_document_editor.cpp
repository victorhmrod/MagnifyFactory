// Standalone smoke test: drives the real JobManager + PdfEngine through a
// "documentEdit" job (reorder, delete, rotate, and insert pages from another
// PDF), verifying with real pdftoppm renders and qpdf/pdfinfo page counts —
// not mocked. Not part of ctest; run manually during Document Editor work.
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QImage>
#include <QProcess>
#include <QTimer>
#include <QDebug>
#include <memory>

#include "core/ConversionJob.h"
#include "core/JobManager.h"
#include "engines/pdf/PdfEngine.h"

using magnify::core::ConversionJob;
using magnify::core::JobManager;
using magnify::core::JobStatus;

namespace {

bool runJob(JobManager &manager, std::unique_ptr<ConversionJob> job) {
    // Connect BEFORE addJob(): once the queue is already running (true for
    // every job after the first), addJob() can start and finish a job
    // synchronously right there (e.g. the JPEG->PDF path, which has no
    // QProcess involved) — connecting afterwards would miss the signal.
    ConversionJob *raw = job.get();
    QEventLoop loop;
    bool finished = false;
    bool ok = false;
    QObject::connect(raw, &ConversionJob::statusChanged, &loop, [&](JobStatus status) {
        if (status == JobStatus::Completed || status == JobStatus::Failed || status == JobStatus::Cancelled) {
            finished = true;
            ok = (status == JobStatus::Completed);
            loop.quit();
        }
    });
    QTimer::singleShot(30000, &loop, &QEventLoop::quit);

    manager.addJob(std::move(job));
    manager.startQueue();
    if (!finished) {
        loop.exec();
    }

    if (!finished) {
        fprintf(stderr, "TIMEOUT\n");
        return false;
    }
    if (!ok) {
        fprintf(stderr, "FAILED: %s\n", qPrintable(raw->errorMessage()));
        return false;
    }
    return true;
}

// Renders page `pageNumber` (1-based) of `pdfPath` to a PNG and returns it,
// or a null QImage on failure — used to check both page identity (average
// color) and orientation (width/height, since our rotation test swaps them).
QImage renderPage(const QString &dir, const QString &pdfPath, int pageNumber, const QString &tag) {
    const QString prefix = dir + QStringLiteral("/render_%1").arg(tag);
    QProcess proc;
    proc.start(QStringLiteral("pdftoppm"),
               {QStringLiteral("-png"), QStringLiteral("-singlefile"), QStringLiteral("-f"),
                QString::number(pageNumber), QStringLiteral("-l"), QString::number(pageNumber), pdfPath, prefix});
    proc.waitForFinished(15000);
    if (proc.exitCode() != 0) {
        fprintf(stderr, "pdftoppm failed for page %d of %s: %s\n", pageNumber, qPrintable(pdfPath),
                qPrintable(QString::fromUtf8(proc.readAllStandardError())));
        return {};
    }
    return QImage(prefix + QStringLiteral(".png"));
}

QRgb averageColor(const QImage &img) {
    if (img.isNull())
        return qRgb(0, 0, 0);
    quint64 r = 0, g = 0, b = 0;
    const int step = qMax(1, img.width() / 20);
    int count = 0;
    for (int y = 0; y < img.height(); y += step) {
        for (int x = 0; x < img.width(); x += step) {
            const QRgb px = img.pixel(x, y);
            r += qRed(px);
            g += qGreen(px);
            b += qBlue(px);
            ++count;
        }
    }
    return count == 0 ? qRgb(0, 0, 0) : qRgb(r / count, g / count, b / count);
}

bool closeColor(QRgb a, QRgb b, int tolerance = 40) {
    return qAbs(qRed(a) - qRed(b)) <= tolerance && qAbs(qGreen(a) - qGreen(b)) <= tolerance &&
           qAbs(qBlue(a) - qBlue(b)) <= tolerance;
}

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    const QString dir = QDir::currentPath();
    bool allOk = true;

    magnify::engines::pdf::PdfEngine pdfEngine;
    JobManager manager;
    manager.registerEngine(&pdfEngine);
    manager.setMaxConcurrentJobs(1);

    // Four single-page PDFs (distinct solid colors, 400x300 landscape),
    // merged into one 4-page source PDF: page1=red, page2=green, page3=blue,
    // page4=yellow. Plus a fifth, separate PDF (magenta) to test inserting a
    // page from another document.
    const QList<QPair<QString, QColor>> pages = {
        {"red", QColor(220, 40, 40)},
        {"green", QColor(40, 200, 60)},
        {"blue", QColor(40, 80, 220)},
        {"yellow", QColor(230, 210, 40)},
    };
    QStringList singlePagePdfs;
    for (const auto &[name, color] : pages) {
        QImage img(400, 300, QImage::Format_RGB32);
        img.fill(color);
        const QString pngPath = dir + "/doc_" + name + ".png";
        const QString jpgPath = dir + "/doc_" + name + ".jpg";
        img.save(pngPath, "PNG");
        img.save(jpgPath, "JPEG");

        const QString pdfPath = dir + "/doc_" + name + ".pdf";
        auto job = std::make_unique<ConversionJob>(jpgPath, pdfPath);
        job->setSourceFormat("jpg");
        job->setTargetFormat("pdf");
        job->setEngineName("PDF Tools");
        if (!runJob(manager, std::move(job))) {
            fprintf(stderr, "FAILED building single-page PDF for %s\n", qPrintable(name));
            return 1;
        }
        singlePagePdfs << pdfPath;
    }

    QImage magentaImg(400, 300, QImage::Format_RGB32);
    magentaImg.fill(QColor(200, 40, 200));
    const QString magentaJpg = dir + "/doc_magenta.jpg";
    magentaImg.save(magentaJpg, "JPEG");
    const QString magentaPdf = dir + "/doc_magenta.pdf";
    {
        auto job = std::make_unique<ConversionJob>(magentaJpg, magentaPdf);
        job->setSourceFormat("jpg");
        job->setTargetFormat("pdf");
        job->setEngineName("PDF Tools");
        if (!runJob(manager, std::move(job))) {
            fprintf(stderr, "FAILED building magenta PDF\n");
            return 1;
        }
    }

    // Merge red/green/blue/yellow into one 4-page source document.
    const QString sourcePdf = dir + "/doc_source.pdf";
    {
        auto job = std::make_unique<ConversionJob>(singlePagePdfs.at(0), sourcePdf);
        job->setExtraInputPaths(singlePagePdfs.mid(1));
        job->setSourceFormat("pdf");
        job->setTargetFormat("pdf");
        job->setEngineName("PDF Tools");
        job->setParameters({{"operation", "merge"}});
        if (!runJob(manager, std::move(job))) {
            fprintf(stderr, "FAILED merging source PDF\n");
            return 1;
        }
    }

    // Document edit: keep page 3 (blue) then page 1 (red, rotated 90°), then
    // append page 1 of the magenta PDF (source index 1). Drops pages 2 and 4
    // (green, yellow) entirely — exercises reorder, delete, rotate, and
    // cross-file insertion all in one job.
    const QString editedPdf = dir + "/doc_edited.pdf";
    {
        auto job = std::make_unique<ConversionJob>(sourcePdf, editedPdf);
        job->setExtraInputPaths({magentaPdf});
        job->setSourceFormat("pdf");
        job->setTargetFormat("pdf");
        job->setEngineName("PDF Tools");
        job->setParameters({
            {"operation", "documentEdit"},
            {"pages", QVariantList{
                          QVariantMap{{"source", 0}, {"page", 3}},
                          QVariantMap{{"source", 0}, {"page", 1}},
                          QVariantMap{{"source", 1}, {"page", 1}},
                      }},
            {"rotations", QVariantMap{{"2", 90}}},
        });
        if (!runJob(manager, std::move(job))) {
            fprintf(stderr, "FAILED document edit\n");
            return 1;
        }
    }

    if (!QFileInfo::exists(editedPdf)) {
        fprintf(stderr, "Edited PDF was not produced\n");
        return 1;
    }

    // Verify page count (pdfinfo isn't guaranteed present; qpdf --show-npages is).
    {
        QProcess proc;
        proc.start(QStringLiteral("qpdf"), {QStringLiteral("--show-npages"), editedPdf});
        proc.waitForFinished(10000);
        const int pageCount = QString::fromUtf8(proc.readAllStandardOutput()).trimmed().toInt();
        if (pageCount != 3) {
            fprintf(stderr, "Expected 3 pages, got %d\n", pageCount);
            allOk = false;
        } else {
            fprintf(stderr, "OK: edited PDF has 3 pages\n");
        }
    }

    // Page 1 should be blue (original page 3), unrotated (400x300).
    {
        QImage rendered = renderPage(dir, editedPdf, 1, "p1");
        const QRgb expected = qRgb(40, 80, 220);
        if (rendered.isNull() || !closeColor(averageColor(rendered), expected)) {
            fprintf(stderr, "Page 1 color mismatch (expected blue)\n");
            allOk = false;
        } else if (rendered.width() <= rendered.height()) {
            fprintf(stderr, "Page 1 should still be landscape (400x300), got %dx%d\n", rendered.width(),
                    rendered.height());
            allOk = false;
        } else {
            fprintf(stderr, "OK: page 1 is blue and landscape (%dx%d)\n", rendered.width(), rendered.height());
        }
    }

    // Page 2 should be red (original page 1), rotated 90° — portrait now.
    {
        QImage rendered = renderPage(dir, editedPdf, 2, "p2");
        const QRgb expected = qRgb(220, 40, 40);
        if (rendered.isNull() || !closeColor(averageColor(rendered), expected)) {
            fprintf(stderr, "Page 2 color mismatch (expected red)\n");
            allOk = false;
        } else if (rendered.width() >= rendered.height()) {
            fprintf(stderr, "Page 2 should be portrait after 90-degree rotation, got %dx%d\n", rendered.width(),
                    rendered.height());
            allOk = false;
        } else {
            fprintf(stderr, "OK: page 2 is red and rotated to portrait (%dx%d)\n", rendered.width(),
                    rendered.height());
        }
    }

    // Page 3 should be magenta (from the extra/inserted file), unrotated.
    {
        QImage rendered = renderPage(dir, editedPdf, 3, "p3");
        const QRgb expected = qRgb(200, 40, 200);
        if (rendered.isNull() || !closeColor(averageColor(rendered), expected)) {
            fprintf(stderr, "Page 3 color mismatch (expected magenta, inserted from extra file)\n");
            allOk = false;
        } else {
            fprintf(stderr, "OK: page 3 is magenta, inserted from the extra PDF\n");
        }
    }

    qInfo() << (allOk ? "ALL DOCUMENT EDITOR CHECKS SUCCEEDED" : "SOME DOCUMENT EDITOR CHECKS FAILED");
    return allOk ? 0 : 1;
}
