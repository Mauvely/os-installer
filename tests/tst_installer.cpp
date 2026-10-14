// ─────────────────────────────────────────────────────────────────────────────
//  tst_installer — the install sequence against a scripted runner.
//
//  What it pins: the commands and their order, that the workaround runs on the
//  known bootc failure and on nothing else, that a failure after the root is
//  opened still unmounts and closes it, and that neither the token nor the
//  recovery key reaches the log.
// ─────────────────────────────────────────────────────────────────────────────

#include "core/host.h"
#include "core/installer.h"
#include "fakerunner.h"

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <sys/stat.h>

namespace {

const QString kToken = QStringLiteral("mvd_q3Zt9Xw0yK8Lr2Vb5Nc7Hd1Jf4Gs6Ap0Me3Ui8Oo_-Q");
const QString kKey = QStringLiteral("fvhkcbjd-lnrtigku-bhefjrtv-vncbtgeh-ddilrkvu-ifnhbutr-ecdjgrtn-likdheuf");
const QString kDeployment = QStringLiteral("ostree/deploy/default/deploy/3f2a9c.0");

}  // namespace

class TestInstaller : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    Config m_config;
    FakeRunner m_fake;

    disks::Disk vda() {
        FakeRunner probe;  // not m_fake, whose calls the tests count
        for (const disks::Disk &d : host::listDisks(probe, m_config))
            if (d.path == QLatin1String("/dev/vda")) return d;
        return {};
    }

    InstallRequest request() { return {vda(), QStringLiteral("http://10.0.2.2:8798"), kToken}; }

    QString log() {
        QFile f(m_config.logPath);
        return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
    }

    QString dep(const QString &rel) const { return m_config.mountPoint + QLatin1Char('/') + kDeployment + rel; }

    /** Runs the sequence and returns (succeeded, key or failure message). */
    std::pair<bool, QString> install(const InstallRequest &req) {
        Installer installer(m_fake, m_config);
        QSignalSpy ok(&installer, &Installer::succeeded), bad(&installer, &Installer::failed);
        const bool result = installer.run(req);
        // Exactly one of the two, and it agrees with the return value.
        if (ok.size() + bad.size() != 1 || result != (ok.size() == 1)) return {false, QStringLiteral("signal mismatch")};
        return {result, result ? ok.first().first().toString() : bad.first().first().toString()};
    }

private slots:
    void init() {
        QVERIFY(m_dir.isValid());
        m_config.logPath = m_dir.filePath(QStringLiteral("log/mauvely-installer.log"));
        m_config.mountPoint = m_dir.filePath(QStringLiteral("target"));
        // What `mount` would reveal: a deployment with an etc and an SELinux config.
        QVERIFY(QDir().mkpath(dep(QStringLiteral("/etc/selinux"))));
        QFile cfg(dep(QStringLiteral("/etc/selinux/config")));
        QVERIFY(cfg.open(QIODevice::WriteOnly));
        cfg.write("SELINUX=enforcing\nSELINUXTYPE=targeted\n");
        cfg.close();
        QFile::remove(m_config.logPath);
        m_fake.reset();
    }

    void cleanup() { QDir(m_config.mountPoint).removeRecursively(); }

    void succeedsOnTheLeakedMount() {
        const auto [ok, key] = install(request());
        QVERIFY2(ok, qPrintable(key + "\n" + log()));
        QCOMPARE(key, kKey);

        const QString mnt = m_config.mountPoint;
        const QString d = mnt + QLatin1Char('/') + kDeployment;
        const QStringList expected{
            "findmnt -n -o SOURCE /run/initramfs/live",
            "lsblk " + disks::lsblkArgs().join(' '),
            "podman images --format json",
            "bootc install to-disk --source-imgref containers-storage:localhost/mauvely-os:p6-test --wipe "
            "--block-setup tpm2-luks --filesystem xfs --target-imgref ghcr.io/mauvely/os:stable "
            "--skip-fetch-check /dev/vda",
            "umount /run/bootc/storage",
            "cryptsetup close root",
            "udevadm settle",
            "lsblk --json --tree --output NAME,PATH,TYPE,FSTYPE /dev/vda",
            "systemd-cryptenroll --unlock-tpm2-device=auto --recovery-key /dev/vda4",
            "cryptsetup open --token-only /dev/vda4 mauvely-target",
            "mount /dev/mapper/mauvely-target " + mnt,
            "setfiles -F -r " + d + " " + d + "/etc/selinux/targeted/contexts/files/file_contexts " + d +
                "/etc/mauvely",
            "ls -lZ " + d + "/etc/mauvely",
            "umount " + mnt,
            "cryptsetup close mauvely-target",
        };
        QCOMPARE(m_fake.lines(), expected);

        QFile enrol(dep("/etc/mauvely/enrol"));
        QVERIFY(enrol.open(QIODevice::ReadOnly));
        QCOMPARE(enrol.readAll(), kToken.toLatin1() + '\n');
        QFile server(dep("/etc/mauvely/server"));
        QVERIFY(server.open(QIODevice::ReadOnly));
        QCOMPARE(server.readAll(), QByteArray("http://10.0.2.2:8798\n"));
        struct stat st {};
        QCOMPARE(::stat(QFile::encodeName(dep("/etc/mauvely/enrol")).constData(), &st), 0);
        QCOMPARE(int(st.st_mode & 07777), 0600);

        // Neither secret is in the log, and the log says what happened.
        const QString l = log();
        QVERIFY(!l.isEmpty());
        QVERIFY(!l.contains(kToken));
        QVERIFY(!l.contains(kToken.mid(4)));
        QVERIFY(!l.contains(kKey));
        QVERIFY(!l.contains(kKey.left(8)));
        QVERIFY(l.contains("output withheld"));
        QVERIFY(l.contains("Task Closing root LUKS device failed"));
        QVERIFY(l.contains("Installed."));
    }

    void succeedsCleanly() {
        m_fake.script["bootc"] = [](const QStringList &) {
            return RunResult{0, QStringLiteral("Closing root LUKS device\nInstallation complete!\n")};
        };
        const auto [ok, key] = install(request());
        QVERIFY2(ok, qPrintable(key));
        // No workaround when nothing leaked.
        QVERIFY(!m_fake.lines().contains("umount /run/bootc/storage"));
        QVERIFY(!m_fake.lines().contains("cryptsetup close root"));
    }

    void otherBootcFailureIsAFailure() {
        m_fake.script["bootc"] = [](const QStringList &) {
            return RunResult{1, fixture(QStringLiteral("bootc-tpm-failure.log"))};
        };
        const auto [ok, message] = install(request());
        QVERIFY(!ok);
        QCOMPARE(message, QStringLiteral("Writing Mauvely OS to the disk failed."));
        const QStringList lines = m_fake.lines();
        QVERIFY(!lines.contains("umount /run/bootc/storage"));
        QVERIFY(!lines.contains("cryptsetup close root"));
        for (const QString &l : lines) QVERIFY2(!l.startsWith("systemd-cryptenroll"), qPrintable(l));
        QVERIFY(!QFileInfo::exists(dep("/etc/mauvely/enrol")));
    }

    void failedWorkaroundIsAFailure() {
        m_fake.script["umount"] = [](const QStringList &) { return RunResult{32, "umount: target is busy."}; };
        const auto [ok, message] = install(request());
        QVERIFY(!ok);
        QCOMPARE(message, QStringLiteral("Mauvely OS was written, but the disk could not be released."));
    }

    void diskSwappedIsRefused() {
        InstallRequest req = request();
        req.disk.serial = QStringLiteral("SOMETHING-ELSE");
        const auto [ok, message] = install(req);
        QVERIFY(!ok);
        QCOMPARE(message, QStringLiteral("The disk changed after you chose it. Choose it again."));
        for (const QString &l : m_fake.lines()) QVERIFY2(!l.startsWith("bootc"), qPrintable(l));
    }

    void diskNowMountedIsRefused() {
        m_fake.script["lsblk"] = [](const QStringList &) {
            QString json = fixture(QStringLiteral("lsblk-vm.json"));
            json.replace(QStringLiteral("\"serial\": \"TARGET\""),
                         QStringLiteral("\"serial\": \"TARGET\", \"mountpoint\": \"/mnt\""));
            return RunResult{0, json};
        };
        disks::Disk d;
        d.name = "vda"; d.path = "/dev/vda"; d.serial = "TARGET"; d.sizeBytes = 68719476736;
        const auto [ok, message] = install({d, "http://10.0.2.2:8798", kToken});
        QVERIFY(!ok);
        for (const QString &l : m_fake.lines()) QVERIFY2(!l.startsWith("bootc"), qPrintable(l));
    }

    void noPayloadIsAFailure() {
        m_fake.script["podman"] = [](const QStringList &) { return RunResult{0, "[]"}; };
        const auto [ok, message] = install(request());
        QVERIFY(!ok);
        QCOMPARE(message, QStringLiteral("The install medium carries no OS image."));
    }

    void noRecoveryKeyIsAFailure() {
        m_fake.script["systemd-cryptenroll"] = [](const QStringList &) {
            return RunResult{1, "Failed to unlock disk: No such device"};
        };
        const auto [ok, message] = install(request());
        QVERIFY(!ok);
        QCOMPARE(message, QStringLiteral("Adding a recovery key failed."));
        // The command's output is a secret on success, so it is withheld on
        // failure too: the log says it ran and how it exited, nothing more.
        QVERIFY(!log().contains("No such device"));
    }

    void failureAfterMountClosesEverything() {
        m_fake.script["setfiles"] = [](const QStringList &) { return RunResult{1, "setfiles: bad"}; };
        const auto [ok, message] = install(request());
        QVERIFY(!ok);
        QCOMPARE(message, QStringLiteral("The server and token could not be labelled for SELinux."));
        const QStringList lines = m_fake.lines();
        QCOMPARE(lines.mid(lines.size() - 2),
                 (QStringList{"umount " + m_config.mountPoint, "cryptsetup close mauvely-target"}));
        QVERIFY(!log().contains(kToken));
    }

    void failureToOpenClosesNothing() {
        m_fake.script["cryptsetup"] = [](const QStringList &args) {
            return args.first() == QLatin1String("open") ? RunResult{1, "No usable token"} : RunResult{0, {}};
        };
        const auto [ok, message] = install(request());
        QVERIFY(!ok);
        QCOMPARE(message, QStringLiteral("The new disk could not be unlocked with the TPM."));
        QCOMPARE(m_fake.lines().last(), QStringLiteral("cryptsetup open --token-only /dev/vda4 mauvely-target"));
    }

    void progressIsStreamed() {
        Installer installer(m_fake, m_config);
        QSignalSpy progress(&installer, &Installer::progress), steps(&installer, &Installer::stepStarted);
        QVERIFY(installer.run(request()));
        QVERIFY(progress.size() >= 10);
        QCOMPARE(progress.last().first().toString(),
                 QStringLiteral("error: Installing to disk: Task Closing root LUKS device failed: "
                                "ExitStatus(unix_wait_status(1280))"));
        QCOMPARE(steps.size(), 3);
        for (int i = 0; i < 3; ++i) QCOMPARE(steps.at(i).first().toInt(), i);
        for (const auto &p : progress) QVERIFY(!p.first().toString().contains(kKey));
    }
};

QTEST_GUILESS_MAIN(TestInstaller)
#include "tst_installer.moc"
