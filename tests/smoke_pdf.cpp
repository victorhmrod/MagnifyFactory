// Standalone smoke test: drives the real JobManager + PdfEngine (+ FFmpeg
// engine, for the PNG->JPEG pre-conversion step) through PDF <-> image jobs.
// Not part of ctest — shells out to real pdftoppm/ffmpeg processes and needs
// a synthetic input image; run manually during PDF Tools verification.
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QImage>
#include <QImageReader>
#include <QRandomGenerator>
#include <QTimer>
#include <QDebug>

#include "core/ConversionJob.h"
#include "core/JobManager.h"
#include "engines/ffmpeg/FFmpegMediaEngine.h"
#include "engines/pdf/PdfEngine.h"

using magnify::core::ConversionJob;
using magnify::core::JobManager;
using magnify::core::JobStatus;

namespace {

bool runOneConversion(JobManager &manager, const QString &input, const QString &output, const QString &sourceExt,
                       const QString &targetExt, const QString &engineName) {
    auto job = std::make_unique<ConversionJob>(input, output);
    job->setSourceFormat(sourceExt);
    job->setTargetFormat(targetExt);
    job->setEngineName(engineName);
    ConversionJob *raw = manager.addJob(std::move(job));

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

    manager.startQueue();
    loop.exec();

    if (!finished) {
        fprintf(stderr, "TIMEOUT converting %s -> %s\n", qPrintable(input), qPrintable(output));
        return false;
    }
    if (!ok) {
        fprintf(stderr, "FAILED converting %s -> %s : %s\n", qPrintable(input), qPrintable(output),
                qPrintable(raw->errorMessage()));
        return false;
    }
    if (!QFileInfo::exists(output) || QFileInfo(output).size() == 0) {
        fprintf(stderr, "Output missing or empty: %s\n", qPrintable(output));
        return false;
    }
    fprintf(stderr, "OK: %s -> %s (%lld bytes)\n", qPrintable(input), qPrintable(output),
            (long long)QFileInfo(output).size());
    return true;
}

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    const QString dir = QDir::currentPath();

    QImage img(400, 300, QImage::Format_RGB32);
    img.fill(QColor(70, 130, 220));
    const QString pngPath = dir + "/sample.png";
    if (!img.save(pngPath, "PNG")) {
        fprintf(stderr, "Could not write test PNG\n");
        return 1;
    }

    magnify::engines::ffmpeg::FFmpegMediaEngine ffmpegEngine;
    magnify::engines::pdf::PdfEngine pdfEngine;
    JobManager manager;
    manager.registerEngine(&ffmpegEngine);
    manager.registerEngine(&pdfEngine);
    manager.setMaxConcurrentJobs(1);

    bool allOk = true;

    // PNG -> PDF (exercises the FFmpeg pre-conversion-to-JPEG + PdfImageWriter path)
    const QString pdfPath = dir + "/sample_from_png.pdf";
    allOk &= runOneConversion(manager, pngPath, pdfPath, "png", "pdf", "PDF Tools");

    // PDF -> PNG (exercises pdftoppm rendering), round-tripping the PDF we just made
    const QString roundtripPngPath = dir + "/sample_roundtrip.png";
    allOk &= runOneConversion(manager, pdfPath, roundtripPngPath, "pdf", "png", "PDF Tools");

    // PDF -> JPG as well, for format coverage
    const QString roundtripJpgPath = dir + "/sample_roundtrip.jpg";
    allOk &= runOneConversion(manager, pdfPath, roundtripJpgPath, "pdf", "jpg", "PDF Tools");

    // PDF -> PDF (compress): build a noisy, high-entropy source image first so
    // a quality-reduced JPEG recompression actually shrinks the file.
    QImage noisy(1200, 900, QImage::Format_RGB32);
    for (int y = 0; y < noisy.height(); ++y) {
        for (int x = 0; x < noisy.width(); ++x) {
            noisy.setPixel(x, y,
                           qRgb(QRandomGenerator::global()->bounded(256), QRandomGenerator::global()->bounded(256),
                                QRandomGenerator::global()->bounded(256)));
        }
    }
    const QString noisyPngPath = dir + "/noisy.png";
    noisy.save(noisyPngPath, "PNG");
    const QString noisyPdfPath = dir + "/noisy.pdf";
    allOk &= runOneConversion(manager, noisyPngPath, noisyPdfPath, "png", "pdf", "PDF Tools");

    const QString compressedPdfPath = dir + "/noisy_compressed.pdf";
    allOk &= runOneConversion(manager, noisyPdfPath, compressedPdfPath, "pdf", "pdf", "PDF Tools");
    if (QFileInfo::exists(noisyPdfPath) && QFileInfo::exists(compressedPdfPath)) {
        const qint64 before = QFileInfo(noisyPdfPath).size();
        const qint64 after = QFileInfo(compressedPdfPath).size();
        fprintf(stderr, "Compress: %lld bytes -> %lld bytes (%.1f%% of original)\n", (long long)before,
                (long long)after, 100.0 * after / before);
    }

    // Merge: two single-page PDFs (sample_from_png.pdf, noisy.pdf) -> one
    // 2-page PDF. Uses ConversionJob::extraInputPaths() directly since this
    // doesn't fit runOneConversion's single-input signature.
    {
        const QString mergedPath = dir + "/merged.pdf";
        auto job = std::make_unique<ConversionJob>(pdfPath, mergedPath);
        job->setExtraInputPaths({noisyPdfPath});
        job->setSourceFormat("pdf");
        job->setTargetFormat("pdf");
        job->setEngineName("PDF Tools");
        job->setParameters({{"operation", "merge"}});
        ConversionJob *raw = manager.addJob(std::move(job));

        QEventLoop loop;
        bool finished = false, ok = false;
        QObject::connect(raw, &ConversionJob::statusChanged, &loop, [&](JobStatus status) {
            if (status == JobStatus::Completed || status == JobStatus::Failed) {
                finished = true;
                ok = (status == JobStatus::Completed);
                loop.quit();
            }
        });
        QTimer::singleShot(30000, &loop, &QEventLoop::quit);
        manager.startQueue();
        loop.exec();

        if (finished && ok && QFileInfo::exists(mergedPath)) {
            fprintf(stderr, "OK: merge produced %s (%lld bytes)\n", qPrintable(mergedPath),
                    (long long)QFileInfo(mergedPath).size());
        } else {
            fprintf(stderr, "FAILED merge: %s\n", qPrintable(raw->errorMessage()));
            allOk = false;
        }
    }

    // Split: the 2-page merged.pdf -> merged-page-1.pdf, merged-page-2.pdf.
    {
        const QString mergedPath = dir + "/merged.pdf";
        const QString pattern = dir + "/merged-page-%d.pdf";
        auto job = std::make_unique<ConversionJob>(mergedPath, pattern);
        job->setSourceFormat("pdf");
        job->setTargetFormat("pdf");
        job->setEngineName("PDF Tools");
        job->setParameters({{"operation", "split"}});
        ConversionJob *raw = manager.addJob(std::move(job));

        QEventLoop loop;
        bool finished = false, ok = false;
        QObject::connect(raw, &ConversionJob::statusChanged, &loop, [&](JobStatus status) {
            if (status == JobStatus::Completed || status == JobStatus::Failed) {
                finished = true;
                ok = (status == JobStatus::Completed);
                loop.quit();
            }
        });
        QTimer::singleShot(30000, &loop, &QEventLoop::quit);
        manager.startQueue();
        loop.exec();

        const bool page1 = QFileInfo::exists(dir + "/merged-page-1.pdf");
        const bool page2 = QFileInfo::exists(dir + "/merged-page-2.pdf");
        if (finished && ok && page1 && page2) {
            fprintf(stderr, "OK: split produced merged-page-1.pdf and merged-page-2.pdf\n");
        } else {
            fprintf(stderr, "FAILED split (page1=%d page2=%d): %s\n", page1, page2, qPrintable(raw->errorMessage()));
            allOk = false;
        }
    }

    qInfo() << (allOk ? "ALL PDF CONVERSIONS SUCCEEDED" : "SOME PDF CONVERSIONS FAILED");
    return allOk ? 0 : 1;
}
