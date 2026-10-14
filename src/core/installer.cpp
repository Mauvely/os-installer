#include "core/installer.h"

#include "core/bootc.h"
#include "core/host.h"
#include "core/luks.h"
#include "core/runner.h"
#include "core/target.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QThread>

#include <cstdio>

namespace {
constexpr int kTailLines = 30;
}

Installer::Installer(Runner &runner, const Config &config, QObject *parent)
    : QObject(parent), m_runner(runner), m_config(config) {}

void Installer::start(const InstallRequest &request) {
    QThread *t = QThread::create([this, request] { run(request); });
    connect(t, &QThread::finished, t, &QObject::deleteLater);
    t->start();
}

bool Installer::run(const InstallRequest &request) {
    QDir().mkpath(QFileInfo(m_config.logPath).absolutePath());
    m_log.setFileName(m_config.logPath);
    if (!m_log.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        std::fprintf(stderr, "cannot open %s; logging to stderr only\n", qPrintable(m_config.logPath));
    m_tail.clear();
    m_opened = m_mounted = false;

    QString key, message;
    const bool ok = sequence(request, &key, &message);
    if (!ok) {
        key.clear();
        log(QStringLiteral("Failed: ") + message);
        cleanUp();
    } else {
        log(QStringLiteral("Installed."));
    }
    const QString tail = m_tail.join(QLatin1Char('\n'));
    m_log.close();

    if (ok)
        emit succeeded(key);
    else
        emit failed(message, tail);
    return ok;
}

bool Installer::sequence(const InstallRequest &req, QString *key, QString *message) {
    const disks::Disk &disk = req.disk;
    QString err;

    log(QStringLiteral("Mauvely OS installer %1").arg(QCoreApplication::applicationVersion()));
    log(QStringLiteral("Disk: %1 · %2 · serial %3 · %4 bytes")
            .arg(disk.path, disk.title(), disk.serial, QString::number(disk.sizeBytes)));
    log(QStringLiteral("Server: ") + req.server);  // the token is never logged
    emit stepStarted(CopyStep);

    // 1. Nothing is written unless the disk is still the one that was confirmed:
    // a USB disk swapped since the list was drawn can come back under the same
    // name.
    bool same = false;
    for (const disks::Disk &d : host::listDisks(m_runner, m_config, &err))
        if (d.path == disk.path)
            same = d.eligible() && d.serial == disk.serial && d.model == disk.model &&
                   d.sizeBytes == disk.sizeBytes;
    if (!same) {
        if (!err.isEmpty()) log(err);
        *message = QStringLiteral("The disk changed after you chose it. Choose it again.");
        return false;
    }

    // 2. The payload.
    const Result images = exec(QStringLiteral("podman"),
                               {QStringLiteral("images"), QStringLiteral("--format"), QStringLiteral("json")});
    const QString payload = images.exitCode == 0
                                ? bootc::payloadFromPodmanImages(images.output.toUtf8(), &err)
                                : QString();
    if (payload.isEmpty()) {
        *message = err.isEmpty() ? QStringLiteral("The OS image on the install medium could not be read.") : err;
        return false;
    }
    log(QStringLiteral("Payload: ") + payload);

    // 3. bootc.
    const Result b = exec(QStringLiteral("bootc"), bootc::installArgs(payload, disk.path),
                          /*secret*/ false, /*stream*/ true);
    switch (bootc::classify(b.exitCode, b.output)) {
    case bootc::Outcome::Succeeded:
        break;
    case bootc::Outcome::LeakedMount:
        log(QStringLiteral("bootc left /run/bootc/storage mounted after a complete install. "
                           "Unmounting it and closing the root by hand."));
        for (const QStringList &cmd : bootc::leakedMountWorkaround()) {
            if (exec(cmd.first(), cmd.mid(1)).exitCode != 0) {
                *message = QStringLiteral("Mauvely OS was written, but the disk could not be released.");
                return false;
            }
        }
        break;
    case bootc::Outcome::Failed:
        *message = QStringLiteral("Writing Mauvely OS to the disk failed.");
        return false;
    }

    // 4. The recovery key. The TPM slot is bound to no PCRs yet, so the live
    // system can unlock the volume with it.
    exec(QStringLiteral("udevadm"), {QStringLiteral("settle")});
    const Result parts = exec(QStringLiteral("lsblk"), luks::lsblkArgs(disk.path));
    const QString part = luks::partition(parts.output.toUtf8(), &err);
    if (part.isEmpty()) {
        *message = err;
        return false;
    }
    emit stepStarted(RecoveryKeyStep);
    const Result rk = exec(QStringLiteral("systemd-cryptenroll"), luks::addRecoveryKeyArgs(part),
                           /*secret*/ true);
    *key = luks::recoveryKey(rk.output);
    if (rk.exitCode != 0 || key->isEmpty()) {
        *message = QStringLiteral("Adding a recovery key failed.");
        return false;
    }

    // 5. The enrolment files.
    emit stepStarted(EnrolmentStep);
    if (exec(QStringLiteral("cryptsetup"),
             {QStringLiteral("open"), QStringLiteral("--token-only"), part, m_config.mapperName})
            .exitCode != 0) {
        *message = QStringLiteral("The new disk could not be unlocked with the TPM.");
        return false;
    }
    m_opened = true;
    QDir().mkpath(m_config.mountPoint);
    if (exec(QStringLiteral("mount"),
             {QStringLiteral("/dev/mapper/") + m_config.mapperName, m_config.mountPoint})
            .exitCode != 0) {
        *message = QStringLiteral("The new system could not be mounted.");
        return false;
    }
    m_mounted = true;

    const QString deployment = target::findDeployment(m_config.mountPoint, &err);
    if (deployment.isEmpty()) {
        *message = err;
        return false;
    }
    log(QStringLiteral("Deployment: ") + deployment);
    if (!target::writeEnrolment(deployment, req.server, req.token, &err)) {
        log(err);
        *message = QStringLiteral("The server and token could not be saved.");
        return false;
    }
    log(QStringLiteral("Wrote etc/mauvely/server and etc/mauvely/enrol."));
    if (exec(QStringLiteral("setfiles"), target::setfilesArgs(deployment)).exitCode != 0) {
        *message = QStringLiteral("The server and token could not be labelled for SELinux.");
        return false;
    }
    exec(QStringLiteral("ls"), {QStringLiteral("-lZ"), deployment + QStringLiteral("/etc/mauvely")});

    if (exec(QStringLiteral("umount"), {m_config.mountPoint}).exitCode != 0) {
        *message = QStringLiteral("The new system could not be unmounted.");
        return false;
    }
    m_mounted = false;
    if (exec(QStringLiteral("cryptsetup"), {QStringLiteral("close"), m_config.mapperName}).exitCode != 0) {
        *message = QStringLiteral("The new disk could not be closed.");
        return false;
    }
    m_opened = false;
    return true;
}

Installer::Result Installer::exec(const QString &program, const QStringList &args, bool secret,
                                  bool stream) {
    log(QStringLiteral("$ ") + program + QLatin1Char(' ') + args.join(QLatin1Char(' ')));
    Runner::LineFn onLine;
    if (stream && !secret)
        onLine = [this](const QString &line) {
            log(line);
            emit progress(line);
        };
    const RunResult r = m_runner.run(program, args, onLine);
    if (secret) {
        log(QStringLiteral("(output withheld: it holds a secret)"));
    } else if (!stream) {
        // lsblk's and podman's JSON belong in the file, not on the failure
        // page; a failing command's complaint belongs in both.
        for (const QString &line : r.output.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
            log(line, r.exitCode != 0);
    }
    log(QStringLiteral("exit %1").arg(r.exitCode));
    return {r.exitCode, r.output};
}

void Installer::log(const QString &line, bool toTail) {
    const QString stamped =
        QDateTime::currentDateTimeUtc().toString(QStringLiteral("HH:mm:ss ")) + line;
    if (m_log.isOpen()) {
        m_log.write(stamped.toUtf8() + '\n');
        m_log.flush();
    }
    std::fprintf(stderr, "%s\n", stamped.toLocal8Bit().constData());
    if (!toTail) return;
    m_tail << line;
    while (m_tail.size() > kTailLines) m_tail.removeFirst();
}

void Installer::cleanUp() {
    if (m_mounted) exec(QStringLiteral("umount"), {m_config.mountPoint});
    if (m_opened) exec(QStringLiteral("cryptsetup"), {QStringLiteral("close"), m_config.mapperName});
    m_mounted = m_opened = false;
}
