#pragma once

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

namespace magnify::core {

// Starts an external tool (ffmpeg, qpdf, 7z, soffice, ...), transparently
// routing through `flatpak-spawn --host` when running inside a Flatpak
// sandbox. A sandboxed process can't exec host binaries directly — they
// simply aren't present inside the sandbox's filesystem — but the
// org.freedesktop.Flatpak portal flatpak-spawn talks to can run them on the
// host and pipe stdio back, which is the only practical way for an app
// that shells out to arbitrary user-installed CLI tools (rather than
// bundling them) to work as a Flatpak. No-op outside a sandbox.
class HostProcess {
public:
    static void start(QProcess *process, const QString &program, const QStringList &args) {
        if (isSandboxed()) {
            process->start(QStringLiteral("flatpak-spawn"), QStringList{QStringLiteral("--host"), program} + args);
        } else {
            process->start(program, args);
        }
    }

    // Exposed for callers that need to skip a sandbox-blind filesystem/PATH
    // check of their own (e.g. DocumentEngine looking for soffice) rather
    // than just starting a process.
    // /.flatpak-info is the standard marker file every Flatpak runtime
    // creates inside the sandbox; checked once and cached.
    static bool isSandboxed() {
        static const bool sandboxed = QFileInfo::exists(QStringLiteral("/.flatpak-info"));
        return sandboxed;
    }

    // A writable directory guaranteed to be the same real path whether
    // accessed by in-process code or by a process started via start() above
    // (which, when sandboxed, actually runs on the host). Plain
    // QStandardPaths::TempLocation resolves to /tmp, which Flatpak gives
    // the sandbox its own private tmpfs for regardless of the
    // --filesystem=host permission — a file an in-process host-spawned tool
    // writes there is invisible to in-process code trying to read it back
    // (or vice versa). CacheLocation (~/.var/app/<id>/cache under Flatpak)
    // is a real, bind-mounted directory visible identically on both sides.
    static QString sharedTempDir() {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir().mkpath(dir);
        return dir;
    }
};

} // namespace magnify::core
