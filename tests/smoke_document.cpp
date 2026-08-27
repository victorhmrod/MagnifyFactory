// Standalone smoke test: drives the real JobManager + DocumentEngine through
// a full round trip (txt -> docx -> pdf -> txt) using the actual LibreOffice
// headless CLI (soffice --headless --convert-to). Not part of ctest — needs
// LibreOffice installed; run manually during Document Tools verification.
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QTimer>
#include <cstdio>

#include "core/ConversionJob.h"
#include "core/JobManager.h"
#include "engines/document/DocumentEngine.h"

using magnify::core::ConversionJob;
using magnify::core::JobManager;
using magnify::core::JobStatus;

namespace {

const QString kMarker = QStringLiteral("MagnifyFactory document round-trip marker 4f8c21");

bool runOneConversion(JobManager &manager, const QString &input, const QString &output, const QString &sourceExt,
                       const QString &targetExt) {
    auto job = std::make_unique<ConversionJob>(input, output);
    job->setSourceFormat(sourceExt);
    job->setTargetFormat(targetExt);
    job->setEngineName(QStringLiteral("Document Tools"));
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
    QTimer::singleShot(60000, &loop, &QEventLoop::quit);
    manager.startQueue();
    loop.exec();

    if (!finished || !ok) {
        fprintf(stderr, "FAILED: %s -> %s did not complete: %s\n", qPrintable(input), qPrintable(output),
                qPrintable(raw->errorMessage()));
        return false;
    }
    if (!QFileInfo::exists(output) || QFileInfo(output).size() == 0) {
        fprintf(stderr, "FAILED: %s was not produced or is empty\n", qPrintable(output));
        return false;
    }
    fprintf(stderr, "OK: %s -> %s (%lld bytes)\n", qPrintable(input), qPrintable(output),
            (long long) QFileInfo(output).size());
    return true;
}

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    if (magnify::engines::document::DocumentEngine::sofficeExecutable().isEmpty()) {
        fprintf(stderr, "FAILED: soffice (LibreOffice) was not found on this machine\n");
        return 1;
    }

    const QString dir = QDir::currentPath();
    const QString txtPath = dir + "/document_smoke_src.txt";
    const QString docxPath = dir + "/document_smoke.docx";
    const QString pdfPath = dir + "/document_smoke.pdf";
    const QString txtRoundTripPath = dir + "/document_smoke_roundtrip.txt";

    QFile srcFile(txtPath);
    if (!srcFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        fprintf(stderr, "FAILED: could not create synthetic source txt\n");
        return 1;
    }
    QTextStream(&srcFile) << kMarker << "\n";
    srcFile.close();

    magnify::engines::document::DocumentEngine engine;
    JobManager manager;
    manager.registerEngine(&engine);

    if (!runOneConversion(manager, txtPath, docxPath, "txt", "docx")) return 1;

    JobManager manager2;
    manager2.registerEngine(&engine);
    if (!runOneConversion(manager2, docxPath, pdfPath, "docx", "pdf")) return 1;

    JobManager manager3;
    manager3.registerEngine(&engine);
    if (!runOneConversion(manager3, docxPath, txtRoundTripPath, "docx", "txt")) return 1;

    QFile roundTrip(txtRoundTripPath);
    if (!roundTrip.open(QIODevice::ReadOnly | QIODevice::Text)) {
        fprintf(stderr, "FAILED: could not read round-tripped txt\n");
        return 1;
    }
    const QString roundTripContent = QTextStream(&roundTrip).readAll();
    if (!roundTripContent.contains(kMarker)) {
        fprintf(stderr, "FAILED: marker text did not survive txt -> docx -> txt round trip. Got:\n%s\n",
                qPrintable(roundTripContent));
        return 1;
    }

    fprintf(stderr, "OK: marker text survived the full txt -> docx -> pdf, docx -> txt round trip\n");
    fprintf(stderr, "ALL DOCUMENT CHECKS PASSED\n");
    return 0;
}
