#pragma once

#include <QHash>
#include <QMutex>
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
//
// Detection is slow-ish (several real ffmpeg invocations) and callers are
// expected to kick it off on a background thread (see MainWindow, which uses
// QtConcurrent for this) rather than block the UI. All public methods are
// thread-safe and idempotent: whichever thread gets here first performs the
// real detection under a lock; everyone else just waits for it or reads the
// cached result.
class HardwareAccelerationManager {
public:
    static HardwareAccelerationManager &instance();

    // Runs detection if it hasn't happened yet. Safe to call from any thread,
    // including concurrently — only the first caller does real work.
    void ensureDetected();

    // Vendors whose encoder for `codecFamily` ("h264" or "hevc") was verified
    // to actually encode on this machine. Always includes Cpu. Blocks until
    // detection has completed if it hasn't already.
    QList<HardwareVendor> availableVendors(const QString &codecFamily = QStringLiteral("h264"));

    // Resolves a vendor (or Auto, picking the first verified GPU vendor) to
    // an FFmpeg encoder name for the given codec family. Returns "libx264"/
    // "libx265" for Cpu, or a fallback to CPU if the requested vendor isn't
    // actually usable. Blocks until detection has completed if it hasn't
    // already (e.g. a conversion started before background detection
    // finished still gets a correct answer, just without the UI's help).
    QString encoderFor(HardwareVendor vendor, const QString &codecFamily = QStringLiteral("h264"));

private:
    HardwareAccelerationManager() = default;
    void detectLocked();
    bool verifyEncoder(const QString &encoderName) const;

    QMutex m_mutex;
    bool m_detected = false;
    // key: "<vendor>:<codecFamily>" -> ffmpeg encoder name (empty if unusable)
    QHash<QString, QString> m_verifiedEncoders;
};

} // namespace magnify::hardware
