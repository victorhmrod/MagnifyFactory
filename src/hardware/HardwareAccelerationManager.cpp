#include "HardwareAccelerationManager.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "core/HostProcess.h"

namespace magnify::hardware {

QString hardwareVendorToString(HardwareVendor vendor) {
    switch (vendor) {
        case HardwareVendor::Auto: return QStringLiteral("auto");
        case HardwareVendor::Cpu: return QStringLiteral("cpu");
        case HardwareVendor::Nvidia: return QStringLiteral("nvidia");
        case HardwareVendor::Amd: return QStringLiteral("amd");
        case HardwareVendor::Intel: return QStringLiteral("intel");
    }
    return QStringLiteral("auto");
}

HardwareVendor hardwareVendorFromString(const QString &value) {
    const QString v = value.toLower();
    if (v == QStringLiteral("cpu")) return HardwareVendor::Cpu;
    if (v == QStringLiteral("nvidia")) return HardwareVendor::Nvidia;
    if (v == QStringLiteral("amd")) return HardwareVendor::Amd;
    if (v == QStringLiteral("intel")) return HardwareVendor::Intel;
    return HardwareVendor::Auto;
}

namespace {
QString candidateEncoder(HardwareVendor vendor, const QString &codecFamily) {
    // FFmpeg's naming convention: "<codec>_<vendor api>"
    const QString suffix = vendor == HardwareVendor::Nvidia ? QStringLiteral("nvenc")
                            : vendor == HardwareVendor::Amd  ? QStringLiteral("amf")
                            : vendor == HardwareVendor::Intel ? QStringLiteral("qsv")
                                                               : QString();
    if (suffix.isEmpty()) {
        return QString();
    }
    return codecFamily + QStringLiteral("_") + suffix;
}
} // namespace

HardwareAccelerationManager &HardwareAccelerationManager::instance() {
    static HardwareAccelerationManager manager;
    return manager;
}

bool HardwareAccelerationManager::verifyEncoder(const QString &encoderName) const {
    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        return false;
    }
    const QString outputPath = tempDir.filePath(QStringLiteral("probe.mp4"));

    QProcess process;
    magnify::core::HostProcess::start(
        &process, QStringLiteral("ffmpeg"),
        {QStringLiteral("-y"), QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
         QStringLiteral("-f"), QStringLiteral("lavfi"), QStringLiteral("-i"),
         QStringLiteral("testsrc=duration=1:size=320x240:rate=5"), QStringLiteral("-frames:v"), QStringLiteral("1"),
         QStringLiteral("-c:v"), encoderName, outputPath});
    if (!process.waitForStarted(3000)) {
        return false;
    }
    if (!process.waitForFinished(10000)) {
        process.kill();
        return false;
    }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0 && QFileInfo::exists(outputPath);
}

void HardwareAccelerationManager::detectLocked() {
    // Caller already holds m_mutex. Runs several real ffmpeg processes, so
    // this is the slow part callers should keep off the UI thread.
    for (HardwareVendor vendor : {HardwareVendor::Nvidia, HardwareVendor::Amd, HardwareVendor::Intel}) {
        for (const QString &codecFamily : {QStringLiteral("h264"), QStringLiteral("hevc")}) {
            const QString encoder = candidateEncoder(vendor, codecFamily);
            const QString key = hardwareVendorToString(vendor) + QStringLiteral(":") + codecFamily;
            m_verifiedEncoders.insert(key, verifyEncoder(encoder) ? encoder : QString());
        }
    }
    m_detected = true;
}

void HardwareAccelerationManager::ensureDetected() {
    QMutexLocker locker(&m_mutex);
    if (!m_detected) {
        detectLocked();
    }
}

QList<HardwareVendor> HardwareAccelerationManager::availableVendors(const QString &codecFamily) {
    ensureDetected();
    QMutexLocker locker(&m_mutex);
    QList<HardwareVendor> result{HardwareVendor::Cpu};
    for (HardwareVendor vendor : {HardwareVendor::Nvidia, HardwareVendor::Amd, HardwareVendor::Intel}) {
        const QString key = hardwareVendorToString(vendor) + QStringLiteral(":") + codecFamily;
        if (!m_verifiedEncoders.value(key).isEmpty()) {
            result.append(vendor);
        }
    }
    return result;
}

QString HardwareAccelerationManager::encoderFor(HardwareVendor vendor, const QString &codecFamily) {
    ensureDetected();
    const QString cpuEncoder = codecFamily == QStringLiteral("hevc") ? QStringLiteral("libx265") : QStringLiteral("libx264");

    if (vendor == HardwareVendor::Cpu) {
        return cpuEncoder;
    }

    QMutexLocker locker(&m_mutex);
    if (vendor == HardwareVendor::Auto) {
        for (HardwareVendor candidate : {HardwareVendor::Nvidia, HardwareVendor::Amd, HardwareVendor::Intel}) {
            const QString key = hardwareVendorToString(candidate) + QStringLiteral(":") + codecFamily;
            const QString encoder = m_verifiedEncoders.value(key);
            if (!encoder.isEmpty()) {
                return encoder;
            }
        }
        return cpuEncoder;
    }

    const QString key = hardwareVendorToString(vendor) + QStringLiteral(":") + codecFamily;
    const QString encoder = m_verifiedEncoders.value(key);
    return encoder.isEmpty() ? cpuEncoder : encoder;
}

} // namespace magnify::hardware
