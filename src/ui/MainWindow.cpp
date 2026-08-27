#include "MainWindow.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QCloseEvent>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStatusBar>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QtConcurrentRun>

#include "ConvertDialog.h"
#include "WatchFoldersDialog.h"
#include "core/ConversionJob.h"
#include "core/FormatRegistry.h"
#include "core/JobManager.h"
#include "engines/archive/ArchiveEngine.h"
#include "engines/document/DocumentEngine.h"
#include "engines/ffmpeg/FFmpegMediaEngine.h"
#include "engines/pdf/PdfEngine.h"
#include "hardware/HardwareAccelerationManager.h"
#include "plugins/PluginManager.h"
#include "watch/WatchFolderManager.h"

using magnify::core::ConversionJob;
using magnify::core::FormatCategory;
using magnify::core::FormatRegistry;
using magnify::core::JobManager;
using magnify::core::JobStatus;
using magnify::hardware::HardwareAccelerationManager;
using magnify::hardware::HardwareVendor;
using magnify::hardware::hardwareVendorToString;
using magnify::watch::WatchFolderManager;
using magnify::watch::WatchRule;

namespace magnify::ui {

namespace {
constexpr int ColumnFile = 0;
constexpr int ColumnFormat = 1;
constexpr int ColumnStatus = 2;
constexpr int ColumnProgress = 3;
constexpr int ColumnEta = 4;

constexpr int RoleCategory = Qt::UserRole + 1;
constexpr int RoleJobId = Qt::UserRole + 2; // ConversionJob::id(), stored on the File column's item

QString statusColor(JobStatus status) {
    switch (status) {
        case JobStatus::Completed: return QStringLiteral("#3fb950");
        case JobStatus::Failed: return QStringLiteral("#f85149");
        case JobStatus::Cancelled: return QStringLiteral("#8b949e");
        case JobStatus::Running: return QStringLiteral("#58a6ff");
        default: return QStringLiteral("#c9d1d9");
    }
}
} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    m_ffmpegEngine = std::make_unique<magnify::engines::ffmpeg::FFmpegMediaEngine>();
    m_pdfEngine = std::make_unique<magnify::engines::pdf::PdfEngine>();
    m_archiveEngine = std::make_unique<magnify::engines::archive::ArchiveEngine>();
    m_documentEngine = std::make_unique<magnify::engines::document::DocumentEngine>();
    m_jobManager = std::make_unique<JobManager>();
    m_jobManager->registerEngine(m_ffmpegEngine.get());
    m_jobManager->registerEngine(m_pdfEngine.get());
    m_jobManager->registerEngine(m_archiveEngine.get());
    m_jobManager->registerEngine(m_documentEngine.get());
    m_watchFolderManager = std::make_unique<WatchFolderManager>();
    connect(m_watchFolderManager.get(), &WatchFolderManager::fileDetected, this, &MainWindow::onWatchedFileDetected);

    m_pluginManager = std::make_unique<magnify::plugins::PluginManager>();
    m_pluginManager->loadPluginsFrom(QCoreApplication::applicationDirPath() + QStringLiteral("/plugins"));

    setAcceptDrops(true);
    setWindowTitle(QStringLiteral("MagnifyFactory"));
    resize(980, 620);

    applyDarkTheme();
    buildUi();

    connect(m_jobManager.get(), &JobManager::jobAdded, this, &MainWindow::appendRow);
    connect(m_jobManager.get(), &JobManager::queueChanged, this, &MainWindow::updateStatusBar);

    loadSettings();
    updateStatusBar();
}

MainWindow::~MainWindow() = default;

void MainWindow::applyDarkTheme() {
    setStyleSheet(R"(
        QMainWindow, QWidget { background-color: #1e2126; color: #c9d1d9; font-size: 13px; }
        QListWidget#sidebar { background-color: #17191d; border: none; padding-top: 8px; outline: none; }
        QListWidget#sidebar::item { padding: 10px 16px; border-left: 3px solid transparent; }
        QListWidget#sidebar::item:selected { background-color: #232833; border-left: 3px solid #58a6ff; color: #ffffff; }
        QListWidget#sidebar::item:hover:!selected { background-color: #1f232a; }
        QPushButton#dropZone { background-color: #232833; border: 2px dashed #3a4048; border-radius: 8px; font-size: 13px; }
        QPushButton#dropZone:hover { border-color: #58a6ff; }
        QPushButton { background-color: #2d333b; border: 1px solid #3a4048; border-radius: 4px; padding: 6px 14px; }
        QPushButton:hover { background-color: #363c46; }
        QSpinBox { background-color: #232833; border: 1px solid #3a4048; border-radius: 4px; padding: 4px 8px; }
        QTableWidget { background-color: #1a1d22; gridline-color: #2a2f37; border: 1px solid #2a2f37; border-radius: 4px; }
        QHeaderView::section { background-color: #232833; color: #8b949e; border: none; padding: 6px; }
        QStatusBar { background-color: #17191d; color: #8b949e; border-top: 1px solid #2a2f37; }
    )");
}

void MainWindow::buildUi() {
    auto *central = new QWidget(this);
    auto *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // --- Sidebar: format categories --------------------------------------
    m_sidebar = new QListWidget(central);
    m_sidebar->setObjectName(QStringLiteral("sidebar"));
    m_sidebar->setFixedWidth(170);

    auto addCategoryItem = [this](const QString &label, FormatCategory category) {
        auto *item = new QListWidgetItem(label, m_sidebar);
        item->setData(RoleCategory, static_cast<int>(category));
    };
    addCategoryItem(QStringLiteral("🎬  Video"), FormatCategory::Video);
    addCategoryItem(QStringLiteral("🎵  Audio"), FormatCategory::Audio);
    addCategoryItem(QStringLiteral("🖼  Images"), FormatCategory::Image);
    addCategoryItem(QStringLiteral("📄  PDF"), FormatCategory::Pdf);
    addCategoryItem(QStringLiteral("📝  Documents"), FormatCategory::Document);
    addCategoryItem(QStringLiteral("🗜  Archive"), FormatCategory::Archive);
    m_sidebar->setCurrentRow(0);
    connect(m_sidebar, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *current, QListWidgetItem *) { onCategorySelected(current); });
    rootLayout->addWidget(m_sidebar);

    // --- Main content -------------------------------------------------------
    auto *content = new QWidget(central);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(20, 20, 20, 12);
    contentLayout->setSpacing(12);

    // Drop zone: dropping or picking one or more files immediately opens the
    // format picker popup (once per format category, so a batch of same-type
    // files only asks once) — there is no separate "convert to" combo anymore.
    m_dropZoneButton = new QPushButton(content);
    m_dropZoneButton->setObjectName(QStringLiteral("dropZone"));
    m_dropZoneButton->setFlat(true);
    m_dropZoneButton->setMinimumHeight(140);
    m_dropZoneButton->setCursor(Qt::PointingHandCursor);
    m_dropZoneButton->setText(QStringLiteral("Drag & drop files here, or click to browse"));
    connect(m_dropZoneButton, &QPushButton::clicked, this, [this]() {
        const QStringList paths = QFileDialog::getOpenFileNames(this, QStringLiteral("Select files"));
        if (!paths.isEmpty()) {
            addInputFiles(paths);
        }
    });

    auto *dropRow = new QHBoxLayout();
    dropRow->addWidget(m_dropZoneButton, 1);
    auto *addFolderButton = new QPushButton(QStringLiteral("Add Folder..."), content);
    addFolderButton->setFixedWidth(120);
    connect(addFolderButton, &QPushButton::clicked, this, &MainWindow::addInputFolder);
    dropRow->addWidget(addFolderButton);
    contentLayout->addLayout(dropRow, 1);

    // --- Queue table -------------------------------------------------------
    m_queueTable = new QTableWidget(0, 5, content);
    m_queueTable->setHorizontalHeaderLabels({"File", "Target", "Status", "Progress", "ETA"});
    m_queueTable->horizontalHeader()->setSectionResizeMode(ColumnFile, QHeaderView::Stretch);
    m_queueTable->verticalHeader()->setVisible(false);
    m_queueTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_queueTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_queueTable->setShowGrid(false);
    m_queueTable->setAlternatingRowColors(false);
    contentLayout->addWidget(m_queueTable, 2);

    // --- Queue controls -----------------------------------------------------
    auto *controlsRow = new QHBoxLayout();
    m_startButton = new QPushButton(QStringLiteral("Start Queue"), content);
    connect(m_startButton, &QPushButton::clicked, this, [this]() { m_jobManager->startQueue(); });
    controlsRow->addWidget(m_startButton);

    auto *pauseButton = new QPushButton(QStringLiteral("Pause"), content);
    connect(pauseButton, &QPushButton::clicked, this, [this]() { m_jobManager->pauseQueue(); });
    controlsRow->addWidget(pauseButton);

    controlsRow->addSpacing(12);
    controlsRow->addWidget(new QLabel(QStringLiteral("Concurrent jobs:"), content));
    m_concurrencySpin = new QSpinBox(content);
    m_concurrencySpin->setRange(1, 16);
    m_concurrencySpin->setValue(2);
    connect(m_concurrencySpin, qOverload<int>(&QSpinBox::valueChanged), this,
            [this](int value) { m_jobManager->setMaxConcurrentJobs(value); });
    controlsRow->addWidget(m_concurrencySpin);

    controlsRow->addSpacing(12);
    controlsRow->addWidget(new QLabel(QStringLiteral("Hardware:"), content));
    m_hardwareCombo = new QComboBox(content);
    m_hardwareCombo->addItem(QStringLiteral("Auto"), hardwareVendorToString(HardwareVendor::Auto));
    m_hardwareCombo->addItem(QStringLiteral("CPU"), hardwareVendorToString(HardwareVendor::Cpu));
    m_hardwareCombo->setToolTip(QStringLiteral("Detecting GPU encoders in the background..."));
    controlsRow->addWidget(m_hardwareCombo);

    // GPU-vendor entries are added once background detection finishes (see
    // ctor); "Auto"/"CPU" already work correctly in the meantime since
    // HardwareAccelerationManager blocks and waits if a conversion needs an
    // answer before detection completes.
    connect(&m_hardwareDetectionWatcher, &QFutureWatcher<void>::finished, this,
            &MainWindow::onHardwareDetectionFinished);
    m_hardwareDetectionWatcher.setFuture(
        QtConcurrent::run([]() { HardwareAccelerationManager::instance().ensureDetected(); }));

    auto *watchFoldersButton = new QPushButton(QStringLiteral("Watch Folders..."), content);
    connect(watchFoldersButton, &QPushButton::clicked, this, &MainWindow::openWatchFoldersDialog);
    controlsRow->addWidget(watchFoldersButton);

    controlsRow->addStretch(1);
    contentLayout->addLayout(controlsRow);

    rootLayout->addWidget(content, 1);
    setCentralWidget(central);

    m_statusJobsLabel = new QLabel(this);
    statusBar()->addWidget(m_statusJobsLabel);

    onCategorySelected(m_sidebar->currentItem());

    if (!m_pluginManager->loadedPlugins().isEmpty()) {
        QStringList names;
        for (const auto &loaded : m_pluginManager->loadedPlugins()) {
            names << loaded.plugin->name();
        }
        statusBar()->showMessage(QStringLiteral("Plugins loaded: %1").arg(names.join(QStringLiteral(", "))), 6000);
    }
}

void MainWindow::onHardwareDetectionFinished() {
    populateHardwareCombo();
}

void MainWindow::openWatchFoldersDialog() {
    WatchFoldersDialog dialog(m_watchFolderManager.get(), this);
    dialog.exec();
}

void MainWindow::onWatchedFileDetected(const QString &filePath, const WatchRule &rule) {
    statusBar()->showMessage(
        QStringLiteral("Watch folder: converting %1").arg(QFileInfo(filePath).fileName()), 4000);
    enqueueFile(filePath, rule.targetExt, rule.parameters);
    m_jobManager->startQueue();
}

void MainWindow::populateHardwareCombo() {
    const QString previousSelection =
        !m_pendingHardwareBackend.isEmpty() ? m_pendingHardwareBackend : m_hardwareCombo->currentData().toString();
    const QList<HardwareVendor> detected = HardwareAccelerationManager::instance().availableVendors();

    if (detected.contains(HardwareVendor::Nvidia)) {
        m_hardwareCombo->addItem(QStringLiteral("NVIDIA NVENC"), hardwareVendorToString(HardwareVendor::Nvidia));
    }
    if (detected.contains(HardwareVendor::Amd)) {
        m_hardwareCombo->addItem(QStringLiteral("AMD AMF"), hardwareVendorToString(HardwareVendor::Amd));
    }
    if (detected.contains(HardwareVendor::Intel)) {
        m_hardwareCombo->addItem(QStringLiteral("Intel Quick Sync"), hardwareVendorToString(HardwareVendor::Intel));
    }
    m_hardwareCombo->setToolTip(QString());

    const int restoredIndex = m_hardwareCombo->findData(previousSelection);
    if (restoredIndex >= 0) {
        m_hardwareCombo->setCurrentIndex(restoredIndex);
    }
    m_pendingHardwareBackend.clear();

    if (detected.size() > 1) {
        QStringList gpuNames;
        for (HardwareVendor vendor : detected) {
            if (vendor != HardwareVendor::Cpu) {
                gpuNames << hardwareVendorToString(vendor).toUpper();
            }
        }
        statusBar()->showMessage(
            QStringLiteral("GPU acceleration available: %1").arg(gpuNames.join(QStringLiteral(", "))), 5000);
    }
}

void MainWindow::onCategorySelected(QListWidgetItem *current) {
    if (!current) {
        return;
    }
    m_activeCategory = static_cast<FormatCategory>(current->data(RoleCategory).toInt());
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event) {
    QStringList paths;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            paths << url.toLocalFile();
        }
    }
    if (!paths.isEmpty()) {
        addInputFiles(paths);
    }
}

void MainWindow::openExternalFile(const QString &filePath) {
    show();
    raise();
    activateWindow();
    addInputFile(filePath);
}

void MainWindow::addInputFile(const QString &filePath) {
    addInputFiles({filePath});
}

void MainWindow::addInputFiles(const QStringList &paths) {
    QHash<FormatCategory, QStringList> byCategory;
    QStringList unrecognized;
    for (const QString &path : paths) {
        const QString ext = QFileInfo(path).suffix().toLower();
        const FormatCategory category = FormatRegistry::instance().categoryOf(ext);
        if (category == FormatCategory::Unknown) {
            unrecognized << QFileInfo(path).fileName();
        } else {
            byCategory[category] << path;
        }
    }

    if (!unrecognized.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("MagnifyFactory"),
                              QStringLiteral("Skipped unrecognized file(s):\n%1").arg(unrecognized.join('\n')));
    }

    // One popup per category, not per file, so dropping a folder full of the
    // same file type only asks the user once.
    for (auto it = byCategory.constBegin(); it != byCategory.constEnd(); ++it) {
        const FormatCategory category = it.key();
        const QStringList &files = it.value();

        for (int row = 0; row < m_sidebar->count(); ++row) {
            QListWidgetItem *item = m_sidebar->item(row);
            if (static_cast<FormatCategory>(item->data(RoleCategory).toInt()) == category) {
                m_sidebar->setCurrentItem(item);
                break;
            }
        }

        if (category == FormatCategory::Archive) {
            // Archives are always "extract here" — there's no meaningful
            // convert-to-another-format flow for them.
            const auto reply = QMessageBox::question(
                this, QStringLiteral("MagnifyFactory"),
                QStringLiteral("Extract %1 archive(s) here?").arg(files.size()));
            if (reply == QMessageBox::Yes) {
                for (const QString &file : files) {
                    extractArchive(file);
                }
            }
            continue;
        }

        if (files.size() > 1) {
            const bool offerMerge = category == FormatCategory::Pdf;
            QMessageBox box(this);
            box.setWindowTitle(QStringLiteral("MagnifyFactory"));
            box.setText(QStringLiteral("You selected %1 files. What would you like to do?").arg(files.size()));
            QPushButton *mergeButton =
                offerMerge ? box.addButton(QStringLiteral("Merge into one PDF"), QMessageBox::ActionRole) : nullptr;
            QPushButton *zipButton = box.addButton(QStringLiteral("Compress into ZIP"), QMessageBox::ActionRole);
            box.addButton(QStringLiteral("Handle Individually"), QMessageBox::RejectRole);
            box.exec();

            if (mergeButton && box.clickedButton() == mergeButton) {
                mergePdfs(files);
                continue;
            }
            if (box.clickedButton() == zipButton) {
                compressToZip(files);
                continue;
            }
        }

        const bool isSingleFile = files.size() == 1;
        const QString label = isSingleFile ? files.first() : QStringLiteral("%1 files").arg(files.size());
        ConvertDialog dialog(label, category, this, isSingleFile);
        if (dialog.exec() == QDialog::Accepted && !dialog.selectedFormat().isEmpty()) {
            for (const QString &file : files) {
                enqueueFile(file, dialog.selectedFormat(), dialog.selectedParameters());
            }
        }
    }
}

void MainWindow::mergePdfs(const QStringList &pdfPaths) {
    const QFileInfo firstInfo(pdfPaths.first());
    const QDir outDir = firstInfo.absoluteDir();
    QString outputPath = outDir.filePath(QStringLiteral("Merged (%1 files).pdf").arg(pdfPaths.size()));
    int suffix = 2;
    while (QFileInfo::exists(outputPath)) {
        outputPath = outDir.filePath(QStringLiteral("Merged (%1 files) %2.pdf").arg(pdfPaths.size()).arg(suffix++));
    }

    auto job = std::make_unique<ConversionJob>(pdfPaths.first(), outputPath);
    job->setExtraInputPaths(pdfPaths.mid(1));
    job->setSourceFormat(QStringLiteral("pdf"));
    job->setTargetFormat(QStringLiteral("pdf"));
    job->setEngineName(QStringLiteral("PDF Tools"));
    job->setParameters({{QStringLiteral("operation"), QStringLiteral("merge")}});

    m_jobManager->addJob(std::move(job));
    statusBar()->showMessage(QStringLiteral("Queued: merge %1 PDFs").arg(pdfPaths.size()), 4000);
}

void MainWindow::compressToZip(const QStringList &files) {
    const QFileInfo firstInfo(files.first());
    const QDir outDir = firstInfo.absoluteDir();
    QString outputPath = outDir.filePath(QStringLiteral("Archive (%1 files).zip").arg(files.size()));
    int suffix = 2;
    while (QFileInfo::exists(outputPath)) {
        outputPath = outDir.filePath(QStringLiteral("Archive (%1 files) %2.zip").arg(files.size()).arg(suffix++));
    }

    auto job = std::make_unique<ConversionJob>(files.first(), outputPath);
    job->setExtraInputPaths(files.mid(1));
    job->setSourceFormat(firstInfo.suffix().toLower());
    job->setTargetFormat(QStringLiteral("zip"));
    job->setEngineName(QStringLiteral("Archive Tools"));
    job->setParameters({{QStringLiteral("operation"), QStringLiteral("create")}});

    m_jobManager->addJob(std::move(job));
    statusBar()->showMessage(QStringLiteral("Queued: compress %1 files into ZIP").arg(files.size()), 4000);
}

void MainWindow::extractArchive(const QString &archivePath) {
    const QFileInfo info(archivePath);
    const QString outputDir = info.absoluteDir().filePath(info.completeBaseName());

    auto job = std::make_unique<ConversionJob>(archivePath, outputDir);
    job->setSourceFormat(info.suffix().toLower());
    job->setTargetFormat(QStringLiteral("folder"));
    job->setEngineName(QStringLiteral("Archive Tools"));
    job->setParameters({{QStringLiteral("operation"), QStringLiteral("extract")}});

    m_jobManager->addJob(std::move(job));
    statusBar()->showMessage(QStringLiteral("Queued: extract %1").arg(info.fileName()), 4000);
}

void MainWindow::addInputFolder() {
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Select folder"));
    if (dir.isEmpty()) {
        return;
    }

    const bool recursive = QMessageBox::question(this, QStringLiteral("MagnifyFactory"),
                                                   QStringLiteral("Include files in subfolders too?")) ==
                            QMessageBox::Yes;

    QStringList files;
    QDirIterator it(dir, QDir::Files,
                     recursive ? QDirIterator::Subdirectories : QDirIterator::IteratorFlag::NoIteratorFlags);
    while (it.hasNext()) {
        files << it.next();
    }

    if (files.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("MagnifyFactory"), QStringLiteral("No files found in that folder."));
        return;
    }
    addInputFiles(files);
}

void MainWindow::enqueueFile(const QString &inputPath, const QString &targetExt, const QVariantMap &presetParameters) {
    // Export next to the source file instead of a separate output folder.
    const QFileInfo inputInfo(inputPath);
    const QDir outDir = inputInfo.absoluteDir();

    if (presetParameters.value(QStringLiteral("operation")).toString() == QStringLiteral("split")) {
        // One PDF -> many; qpdf --split-pages fills in "%d" per output file.
        const QString outputPath = outDir.filePath(inputInfo.completeBaseName() + QStringLiteral("-page-%d.pdf"));
        auto job = std::make_unique<ConversionJob>(inputPath, outputPath);
        job->setSourceFormat(QStringLiteral("pdf"));
        job->setTargetFormat(QStringLiteral("pdf"));
        job->setEngineName(QStringLiteral("PDF Tools"));
        job->setParameters(presetParameters);
        m_jobManager->addJob(std::move(job));
        statusBar()->showMessage(QStringLiteral("Queued: split %1").arg(inputInfo.fileName()), 4000);
        return;
    }

    QString outputPath = outDir.filePath(inputInfo.completeBaseName() + QStringLiteral(".") + targetExt);

    // Never let the output overwrite the input file it comes from.
    if (QFileInfo(outputPath).absoluteFilePath().compare(inputInfo.absoluteFilePath(), Qt::CaseInsensitive) == 0) {
        outputPath = outDir.filePath(inputInfo.completeBaseName() + QStringLiteral(" (converted).") + targetExt);
    }

    const QString sourceExt = inputInfo.suffix().toLower();
    const bool isPdfJob = sourceExt == QStringLiteral("pdf") || targetExt == QStringLiteral("pdf");
    const bool isDocumentJob = FormatRegistry::instance().categoryOf(sourceExt) == FormatCategory::Document ||
                                FormatRegistry::instance().categoryOf(targetExt) == FormatCategory::Document;

    QString engineName = QStringLiteral("FFmpeg");
    if (isDocumentJob) {
        engineName = QStringLiteral("Document Tools");
    } else if (isPdfJob) {
        engineName = QStringLiteral("PDF Tools");
    }

    // The Hardware combo picks a default; a preset's own parameters (if any)
    // take priority since it was chosen specifically for this conversion.
    QVariantMap parameters{{QStringLiteral("hardwareBackend"), m_hardwareCombo->currentData()}};
    for (auto it = presetParameters.constBegin(); it != presetParameters.constEnd(); ++it) {
        parameters.insert(it.key(), it.value());
    }

    auto job = std::make_unique<ConversionJob>(inputPath, outputPath);
    job->setSourceFormat(sourceExt);
    job->setTargetFormat(targetExt);
    job->setEngineName(engineName);
    job->setParameters(parameters);

    m_jobManager->addJob(std::move(job));
    statusBar()->showMessage(QStringLiteral("Queued: %1 → %2").arg(inputInfo.fileName(), targetExt.toUpper()), 4000);
}

void MainWindow::appendRow(ConversionJob *job) {
    const int row = m_queueTable->rowCount();
    m_queueTable->insertRow(row);
    auto *fileItem = new QTableWidgetItem(QFileInfo(job->inputPath()).fileName());
    fileItem->setData(RoleJobId, job->id());
    m_queueTable->setItem(row, ColumnFile, fileItem);
    m_queueTable->setItem(row, ColumnFormat, new QTableWidgetItem(job->targetFormat().toUpper()));
    m_queueTable->setItem(row, ColumnStatus, new QTableWidgetItem(magnify::core::jobStatusToString(job->status())));
    m_queueTable->setItem(row, ColumnProgress, new QTableWidgetItem(QStringLiteral("0%")));
    m_queueTable->setItem(row, ColumnEta, new QTableWidgetItem(QStringLiteral("-")));

    connect(job, &ConversionJob::statusChanged, this, [this, job](magnify::core::JobStatus) { refreshRow(job); });
    connect(job, &ConversionJob::progressChanged, this, [this, job](int) { refreshRow(job); });
}

void MainWindow::refreshRow(ConversionJob *job) {
    for (int row = 0; row < m_queueTable->rowCount(); ++row) {
        if (m_queueTable->item(row, ColumnFile)->data(RoleJobId).toUuid() == job->id()) {
            auto *statusItem = m_queueTable->item(row, ColumnStatus);
            statusItem->setText(magnify::core::jobStatusToString(job->status()));
            statusItem->setForeground(QColor(statusColor(job->status())));
            m_queueTable->item(row, ColumnProgress)->setText(QStringLiteral("%1%").arg(job->progressPercent()));
            m_queueTable->item(row, ColumnEta)
                ->setText(job->etaSeconds() >= 0 ? QStringLiteral("%1s").arg(job->etaSeconds())
                                                  : QStringLiteral("-"));
            if (job->status() == JobStatus::Failed && !job->errorMessage().isEmpty()) {
                QMessageBox::warning(this, QStringLiteral("Conversion failed"),
                                      QStringLiteral("%1\n\nDetails:\n%2")
                                          .arg(QFileInfo(job->inputPath()).fileName(), job->errorMessage()));
            }
            return;
        }
    }
}

void MainWindow::loadSettings() {
    QSettings settings;

    restoreGeometry(settings.value(QStringLiteral("window/geometry")).toByteArray());

    m_concurrencySpin->setValue(settings.value(QStringLiteral("queue/concurrency"), 2).toInt());

    m_pendingHardwareBackend = settings.value(QStringLiteral("queue/hardwareBackend")).toString();
    if (!m_pendingHardwareBackend.isEmpty()) {
        const int index = m_hardwareCombo->findData(m_pendingHardwareBackend);
        if (index >= 0) {
            m_hardwareCombo->setCurrentIndex(index);
            m_pendingHardwareBackend.clear();
        }
        // If the vendor isn't in the combo yet (GPU detection still running),
        // populateHardwareCombo() picks up m_pendingHardwareBackend once it finishes.
    }

    const int ruleCount = settings.beginReadArray(QStringLiteral("watchFolders/rules"));
    for (int i = 0; i < ruleCount; ++i) {
        settings.setArrayIndex(i);
        watch::WatchRule rule;
        rule.folderPath = settings.value(QStringLiteral("folderPath")).toString();
        rule.targetExt = settings.value(QStringLiteral("targetExt")).toString();
        rule.parameters = settings.value(QStringLiteral("parameters")).toMap();
        rule.enabled = settings.value(QStringLiteral("enabled"), true).toBool();
        if (!rule.folderPath.isEmpty() && QDir(rule.folderPath).exists()) {
            m_watchFolderManager->addRule(rule);
        }
    }
    settings.endArray();
}

void MainWindow::saveSettings() {
    QSettings settings;

    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("queue/concurrency"), m_concurrencySpin->value());
    settings.setValue(QStringLiteral("queue/hardwareBackend"), m_hardwareCombo->currentData().toString());

    const QVector<WatchRule> &rules = m_watchFolderManager->rules();
    settings.beginWriteArray(QStringLiteral("watchFolders/rules"));
    for (int i = 0; i < rules.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("folderPath"), rules.at(i).folderPath);
        settings.setValue(QStringLiteral("targetExt"), rules.at(i).targetExt);
        settings.setValue(QStringLiteral("parameters"), rules.at(i).parameters);
        settings.setValue(QStringLiteral("enabled"), rules.at(i).enabled);
    }
    settings.endArray();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::updateStatusBar() {
    int running = 0, queued = 0;
    for (ConversionJob *job : m_jobManager->jobs()) {
        if (job->status() == JobStatus::Running || job->status() == JobStatus::Preparing) {
            ++running;
        } else if (job->status() == JobStatus::Queued) {
            ++queued;
        }
    }
    m_statusJobsLabel->setText(QStringLiteral("Active jobs: %1    Queued: %2    Total: %3")
                                    .arg(running)
                                    .arg(queued)
                                    .arg(m_jobManager->jobs().size()));
}

} // namespace magnify::ui
