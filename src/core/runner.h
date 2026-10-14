#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  Runner — the seam between the installer and the commands it runs.
//
//  Every program the installer starts (lsblk, bootc, cryptsetup, mount, …)
//  goes through `Runner::run()`. The live binary uses `ProcessRunner`; the
//  tests use a scripted fake (tests/fakerunner.h), so the whole install
//  sequence runs without root and without a disk.
//
//  `run()` blocks. The install sequence calls it from its own thread, and the
//  UI thread only for the quick ones (lsblk, findmnt, systemctl reboot).
//
//  The runner never logs. Whether a command's output may be written down is
//  the caller's decision: `systemd-cryptenroll --recovery-key` prints the
//  recovery key, and that must reach the screen and nothing else.
// ─────────────────────────────────────────────────────────────────────────────

#include <QString>
#include <QStringList>

#include <functional>

struct RunResult {
    int exitCode = -1;  // -1 when it did not start, or did not exit normally
    QString output;     // stdout and stderr, merged
};

class Runner {
public:
    using LineFn = std::function<void(const QString &line)>;

    virtual ~Runner() = default;

    /** Runs `program` to completion. `onLine`, if set, sees each output line
     *  as it arrives, for progress. */
    virtual RunResult run(const QString &program, const QStringList &args,
                          const LineFn &onLine = {}) = 0;
};

class ProcessRunner : public Runner {
public:
    RunResult run(const QString &program, const QStringList &args,
                  const LineFn &onLine = {}) override;
};
