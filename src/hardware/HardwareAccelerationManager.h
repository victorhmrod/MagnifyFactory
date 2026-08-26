#pragma once

#include <QHash>
#include <QString>

namespace magnify::hardware {

enum class HardwareVendor {
    Auto,   // let the manager pick the best verified backend
    Cpu,
    Nvidia,
    Amd,
    Intel
};

QString hardwareVendorToString(HardwareVendor vendor);
HardwareVendor hardwareVendorFromString(const QString &value);

// Detects which GPU encoders are actually usable, not just compiled into
// FFmpeg: a build can list h264_nvenc while no NVIDIA GPU/driver is present.
// Detection runs a real, tiny (1-frame) test encode for each compiled
// candidate and caches the verified result for the lifetime of the process.
class HardwareAccelerationManager {
public:
    static HardwareAccelerationManager &instance();

    // Vendors whose encoder for `codecFamily` ("h264" or "hevc") was verified
    // to actually encode on this machine. Always includes Cpu.
    QList<HardwareVendor> availableVendors(const QString &codecFamily = QStringLiteral("h264"));

    // Resolves a vendor (or Auto, picking the first verified GPU vendor) to
    // an FFmpeg encoder name for the given codec family. Returns "libx264"/
    // "libx265" for Cpu, or a fallback to CPU if the requested vendor isn't
    // actually usable.
    QString encoderFor(HardwareVendor vendor, const QString &codecFamily = QStringLiteral("h264"));

private:
    HardwareAccelerationManager() = default;
    void detectIfNeeded();
    bool verifyEncoder(const QString &encoderName) const;

    bool m_detected = false;
    // key: "<vendor>:<codecFamily>" -> ffmpeg encoder name (empty if unusable)
    QHash<QString, QString> m_verifiedEncoders;
};

} // namespace magnify::hardware
