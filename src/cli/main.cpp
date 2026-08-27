// magnify: command-line interface sharing the exact same core, engines,
// hardware detection, and presets the GUI uses (magnify_core, magnify_engines,
// magnify_hardware, magnify_presets) — no separate conversion logic here,
// only argument parsing and wiring jobs into the same JobManager.
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QTextStream>

#include "core/ConversionJob.h"
#include "core/FormatRegistry.h"
#include "core/JobManager.h"
#include "engines/document/DocumentEngine.h"
#include "engines/ffmpeg/FFmpegMediaEngine.h"
#include "engines/pdf/PdfEngine.h"
#include "presets/PresetRegistry.h"

using magnify::core::ConversionJob;
using magnify::core::FormatCategory;
using magnify::core::FormatRegistry;
using magnify::core::JobManager;
using magnify::core::JobStatus;
using magnify::presets::PresetRegistry;

namespace {

QTextStream out(stdout);
QTextStream errOut(stderr);

// Expands a wildcard argument (needed on Windows, where cmd.exe doesn't
// expand globs the way a POSIX shell does) into real file paths. Arguments
// that are already real paths pass through unchanged.
QStringList expandInputs(const QStringList &args) {
    QStringList result;
    for (const QString &arg : args) {
        if (!arg.contains('*') && !arg.contains('?')) {
            result << arg;
            continue;
        }
        const QFileInfo info(arg);
        const QDir dir = info.absoluteDir();
        for (const QString &match : dir.entryList({info.fileName()}, QDir::Files)) {
            result << dir.filePath(match);
        }
    }
    return result;
}

// Runs every queued job to completion, printing one line per job as it
// finishes. Returns true only if every job completed successfully.
bool runQueueToCompletion(JobManager &manager) {
    bool allOk = true;
    int remaining = manager.jobs().size();
    if (remaining == 0) {
        return true;
    }

    QEventLoop loop;
    for (ConversionJob *job : manager.jobs()) {
        QObject::connect(job, &ConversionJob::statusChanged, &loop, [&, job](JobStatus status) {
            if (status != JobStatus::Completed && status != JobStatus::Failed &&
                status != JobStatus::Cancelled) {
                return;
            }
            if (status == JobStatus::Completed) {
                out << "OK    " << QFileInfo(job->inputPath()).fileName() << " -> " << job->outputPath() << "\n";
            } else {
                allOk = false;
                errOut << "FAIL  " << QFileInfo(job->inputPath()).fileName() << ": " << job->errorMessage() << "\n";
            }
            out.flush();
            if (--remaining == 0) {
                loop.quit();
            }
        });
    }

    manager.startQueue();
    loop.exec();
    return allOk;
}

QString outputPathFor(const QString &inputPath, const QString &targetExt, const QString &outputDir) {
    const QFileInfo inputInfo(inputPath);
    const QDir dir = outputDir.isEmpty() ? inputInfo.absoluteDir() : QDir(outputDir);
    QString path = dir.filePath(inputInfo.completeBaseName() + QStringLiteral(".") + targetExt);
    if (QFileInfo(path).absoluteFilePath().compare(inputInfo.absoluteFilePath(), Qt::CaseInsensitive) == 0) {
        path = dir.filePath(inputInfo.completeBaseName() + QStringLiteral(" (converted).") + targetExt);
    }
    return path;
}

// Shared by "convert", "image", and plain "pdf --to <format>": builds and
// queues one job per input file with the given target format and params.
void enqueueConversions(JobManager &manager, const QStringList &files, const QString &targetExt,
                         const QVariantMap &params, const QString &outputDir, const QString &engineName) {
    for (const QString &file : files) {
        const QString outPath = outputPathFor(file, targetExt, outputDir);
        auto job = std::make_unique<ConversionJob>(file, outPath);
        job->setSourceFormat(QFileInfo(file).suffix().toLower());
        job->setTargetFormat(targetExt);
        job->setEngineName(engineName);
        job->setParameters(params);
        manager.addJob(std::move(job));
    }
}

int runConvert(const QStringList &args, bool isPdfCommand) {
    QCommandLineParser parser;
    parser.setApplicationDescription(isPdfCommand ? "Convert or manipulate PDF files."
                                                    : "Convert video, audio, or image files.");
    parser.addHelpOption();
    parser.addPositionalArgument("files", "Input file(s) — wildcards allowed.", "<files...>");

    QCommandLineOption toOption("to", "Target format extension (mp4, mp3, webp, png, ...).", "ext");
    QCommandLineOption codecOption("codec", "Video codec: h264 or h265.", "codec");
    QCommandLineOption crfOption("crf", "Video CRF quality (lower = better, ffmpeg default scale).", "n");
    QCommandLineOption qualityOption("quality", "Image/WebP quality, 0-100.", "n");
    QCommandLineOption widthOption("width", "Target video/output width in pixels.", "px");
    QCommandLineOption heightOption("height", "Target video/output height in pixels.", "px");
    QCommandLineOption videoBitrateOption("video-bitrate", "Video bitrate, e.g. 6000k.", "rate");
    QCommandLineOption audioBitrateOption("audio-bitrate", "Audio bitrate, e.g. 192k.", "rate");
    QCommandLineOption hardwareOption("hardware", "auto|cpu|nvidia|amd|intel", "vendor", "auto");
    QCommandLineOption presetOption("preset", "Named preset (see --list-presets).", "name");
    QCommandLineOption listPresetsOption("list-presets", "List available presets and exit.");
    QCommandLineOption outputDirOption(QStringList{"o", "output-dir"}, "Output directory (default: next to source).",
                                        "dir");
    QCommandLineOption dpiOption("dpi", "PDF -> image render resolution.", "n", "150");
    QCommandLineOption mergeOption("merge", "Merge all input PDFs into one file (requires -o).");
    QCommandLineOption splitOption("split", "Split a PDF into one file per page.");
    QCommandLineOption concurrencyOption("jobs", "Max concurrent conversions.", "n", "2");

    parser.addOptions({toOption, codecOption, crfOption, qualityOption, widthOption, heightOption,
                        videoBitrateOption, audioBitrateOption, hardwareOption, presetOption, listPresetsOption,
                        outputDirOption, concurrencyOption});
    if (isPdfCommand) {
        parser.addOptions({dpiOption, mergeOption, splitOption});
    }
    parser.process(args);

    if (parser.isSet(listPresetsOption)) {
        for (const auto &preset : PresetRegistry::instance().all()) {
            out << preset.name << " (" << preset.targetFormat << ")\n";
        }
        return 0;
    }

    const QStringList files = expandInputs(parser.positionalArguments());
    if (files.isEmpty()) {
        errOut << "No input files given.\n";
        parser.showHelp(1);
    }

    magnify::engines::ffmpeg::FFmpegMediaEngine ffmpegEngine;
    magnify::engines::pdf::PdfEngine pdfEngine;
    magnify::engines::document::DocumentEngine documentEngine;
    JobManager manager;
    manager.registerEngine(&ffmpegEngine);
    manager.registerEngine(&pdfEngine);
    manager.registerEngine(&documentEngine);
    manager.setMaxConcurrentJobs(parser.value(concurrencyOption).toInt());

    if (isPdfCommand && parser.isSet(mergeOption)) {
        if (!parser.isSet(outputDirOption) || files.size() < 2) {
            errOut << "--merge needs at least 2 input PDFs and -o/--output-dir <output file path>.\n";
            return 1;
        }
        auto job = std::make_unique<ConversionJob>(files.first(), parser.value(outputDirOption));
        job->setExtraInputPaths(files.mid(1));
        job->setSourceFormat("pdf");
        job->setTargetFormat("pdf");
        job->setEngineName("PDF Tools");
        job->setParameters({{"operation", "merge"}});
        manager.addJob(std::move(job));
        return runQueueToCompletion(manager) ? 0 : 1;
    }

    if (isPdfCommand && parser.isSet(splitOption)) {
        for (const QString &file : files) {
            const QFileInfo info(file);
            const QString pattern = info.absoluteDir().filePath(info.completeBaseName() + "-page-%d.pdf");
            auto job = std::make_unique<ConversionJob>(file, pattern);
            job->setSourceFormat("pdf");
            job->setTargetFormat("pdf");
            job->setEngineName("PDF Tools");
            job->setParameters({{"operation", "split"}});
            manager.addJob(std::move(job));
        }
        return runQueueToCompletion(manager) ? 0 : 1;
    }

    QString targetExt;
    QVariantMap params;
    if (parser.isSet(presetOption)) {
        const QString presetName = parser.value(presetOption);
        bool found = false;
        for (const auto &preset : PresetRegistry::instance().all()) {
            if (preset.name.compare(presetName, Qt::CaseInsensitive) == 0) {
                targetExt = preset.targetFormat;
                params = preset.parameters;
                found = true;
                break;
            }
        }
        if (!found) {
            errOut << "Unknown preset: " << presetName << " (see --list-presets)\n";
            return 1;
        }
    }
    if (parser.isSet(toOption)) {
        targetExt = parser.value(toOption);
    }
    if (targetExt.isEmpty()) {
        errOut << "Specify --to <format> or --preset <name>.\n";
        parser.showHelp(1);
    }

    if (parser.isSet(codecOption)) {
        const QString codec = parser.value(codecOption).toLower();
        params["videoCodec"] = (codec == "h265" || codec == "hevc") ? "libx265" : "libx264";
    }
    if (parser.isSet(crfOption)) params["crf"] = parser.value(crfOption).toInt();
    if (parser.isSet(qualityOption)) {
        params["quality"] = parser.value(qualityOption).toInt();
        params["jpegQuality"] = parser.value(qualityOption).toInt() / 40 + 1; // rough 0-100 -> ffmpeg 1-3 mapping
    }
    if (parser.isSet(widthOption)) params["width"] = parser.value(widthOption).toInt();
    if (parser.isSet(heightOption)) params["height"] = parser.value(heightOption).toInt();
    if (parser.isSet(videoBitrateOption)) params["videoBitrate"] = parser.value(videoBitrateOption);
    if (parser.isSet(audioBitrateOption)) params["audioBitrate"] = parser.value(audioBitrateOption);
    if (isPdfCommand) params["dpi"] = parser.value(dpiOption).toInt();
    params["hardwareBackend"] = parser.value(hardwareOption);

    const bool isPdfJob = files.first().endsWith(".pdf", Qt::CaseInsensitive) || targetExt == "pdf";
    const bool isDocumentJob =
        FormatRegistry::instance().categoryOf(QFileInfo(files.first()).suffix().toLower()) ==
            FormatCategory::Document ||
        FormatRegistry::instance().categoryOf(targetExt) == FormatCategory::Document;
    const QString engineName = isDocumentJob ? "Document Tools" : (isPdfJob ? "PDF Tools" : "FFmpeg");
    enqueueConversions(manager, files, targetExt, params, parser.value(outputDirOption), engineName);

    return runQueueToCompletion(manager) ? 0 : 1;
}

void printUsage() {
    errOut << "magnify — MagnifyFactory command-line interface\n\n"
           << "Usage:\n"
           << "  magnify convert <files...> --to <ext> [options]\n"
           << "  magnify image <files...> --to <ext> [options]      (alias of convert)\n"
           << "  magnify pdf <files...> --to <ext> [--dpi n]\n"
           << "  magnify pdf <files...> --merge -o <output.pdf>\n"
           << "  magnify pdf <file> --split\n\n"
           << "Run 'magnify <command> --help' for command-specific options.\n";
}

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("magnify");
    QCoreApplication::setApplicationVersion("0.1.0");

    QStringList args = QCoreApplication::arguments();
    if (args.size() < 2 || args.at(1) == "--help" || args.at(1) == "-h") {
        printUsage();
        return args.size() < 2 ? 1 : 0;
    }

    const QString command = args.takeAt(1); // remove the subcommand, keep argv[0] for QCommandLineParser
    if (command == "convert" || command == "image") {
        return runConvert(args, false);
    }
    if (command == "pdf") {
        return runConvert(args, true);
    }

    errOut << "Unknown command: " << command << "\n\n";
    printUsage();
    return 1;
}
