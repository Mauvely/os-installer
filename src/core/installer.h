#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  Installer — the sequence that writes Mauvely OS to a disk.
//
//  1. Check the chosen disk is still there, still the same, still unused.
//  2. Find the OS image the ISO carries.
//  3. `bootc install to-disk … --wipe --block-setup tpm2-luks`, from the live
//     system, recovering from the known leaked-mount failure (core/bootc.h).
//  4. Add a LUKS recovery key, unlocking with the TPM (core/luks.h).
//  5. Open and mount the new root, write /etc/mauvely/server and
//     /etc/mauvely/enrol into the deployment, label them, unmount, close.
//
//  The image owns the rest, on first boot: binding the TPM slot to PCR 7 and
//  dropping `headless=true`. Plasma Setup still runs and makes the local
//  account; the installer does not write /etc/plasma-setup-done.
//
//  Two secrets pass through here and neither is ever logged: the token, which
//  is written only to the target's `enrol`, and the recovery key, which goes
//  only to `succeeded()` and from there to the screen.
// ─────────────────────────────────────────────────────────────────────────────

#include "core/config.h"
#include "core/disks.h"

#include <QFile>
#include <QObject>
#include <QString>
#include <QStringList>

class Runner;

struct InstallRequest {
    disks::Disk disk;
    QString server;  // normalised
    QString token;   // well-formed
};

class Installer : public QObject {
    Q_OBJECT
public:
    enum Step { CopyStep = 0, RecoveryKeyStep = 1, EnrolmentStep = 2 };
    Q_ENUM(Step)

    Installer(Runner &runner, const Config &config, QObject *parent = nullptr);

    /** Runs `run()` on a thread of its own; the signals arrive queued. */
    void start(const InstallRequest &request);

    /** The whole sequence, blocking. Emits exactly one of `succeeded()` and
     *  `failed()`, and returns whether it succeeded. */
    bool run(const InstallRequest &request);

signals:
    void stepStarted(int step);
    void progress(const QString &line);
    void succeeded(const QString &recoveryKey);
    /** `message` is one sentence for the screen; `logTail` the last lines of
     *  the log, which hold neither secret. */
    void failed(const QString &message, const QString &logTail);

private:
    bool sequence(const InstallRequest &request, QString *recoveryKey, QString *message);
    /** Runs and logs a command, and its output unless `secret`. */
    struct Result {
        int exitCode;
        QString output;
    };
    Result exec(const QString &program, const QStringList &args, bool secret = false,
                bool stream = false);
    /** To the log file and stderr, and to the tail the failure page shows
     *  unless `toTail` is false. */
    void log(const QString &line, bool toTail = true);
    void cleanUp();

    Runner &m_runner;
    Config m_config;
    QFile m_log;
    QStringList m_tail;
    bool m_opened = false;
    bool m_mounted = false;
};
