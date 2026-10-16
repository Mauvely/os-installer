// ─────────────────────────────────────────────────────────────────────────────
//  tst_logic — the pure parts: lsblk, the token, the kernel arguments, bootc's
//  outcome, the payload, the LUKS partition, the recovery key and the files
//  written into the deployment.
// ─────────────────────────────────────────────────────────────────────────────

#include "core/bootc.h"
#include "core/cmdline.h"
#include "core/config.h"
#include "core/copyprogress.h"
#include "core/disks.h"
#include "core/enrolment.h"
#include "core/host.h"
#include "core/luks.h"
#include "core/target.h"
#include "fakerunner.h"

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

#include <sys/stat.h>

using disks::Exclusion;

namespace {

const disks::Disk *byName(const QList<disks::Disk> &all, const QString &name) {
    for (const disks::Disk &d : all)
        if (d.name == name) return &d;
    return nullptr;
}

// 32 bytes, so 43 base64url characters with the last one carrying 4 bits.
const QString kToken = QStringLiteral("mvd_q3Zt9Xw0yK8Lr2Vb5Nc7Hd1Jf4Gs6Ap0Me3Ui8Oo_-Q");

bool writeFile(const QString &path, const QByteArray &bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size();
}

int modeOf(const QString &path) {
    struct stat st {};
    return ::stat(QFile::encodeName(path).constData(), &st) == 0 ? int(st.st_mode & 07777) : -1;
}

}  // namespace

class TestLogic : public QObject {
    Q_OBJECT

private slots:
    // ── disks ───────────────────────────────────────────────────────────────

    void disksVm() {
        QString err;
        const auto all = disks::parse(fixture("lsblk-vm.json").toUtf8(), "/dev/sr0",
                                      "Mauvely-OS-Installer", &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        // Only whole disks: the loop device and the DVD drive are not.
        QCOMPARE(all.size(), 6);
        QVERIFY(!byName(all, "loop0"));
        QVERIFY(!byName(all, "sr0"));

        QCOMPARE(byName(all, "zram0")->exclusion, Exclusion::Virtual);
        QCOMPARE(byName(all, "sdb")->exclusion, Exclusion::Mounted);   // a partition is
        QCOMPARE(byName(all, "sdc")->exclusion, Exclusion::Empty);     // a card reader, no card
        QCOMPARE(byName(all, "sdd")->exclusion, Exclusion::ReadOnly);

        const auto ok = disks::eligible(all);
        QCOMPARE(ok.size(), 2);
        QCOMPARE(ok[0].path, QStringLiteral("/dev/vda"));
        QCOMPARE(ok[0].serial, QStringLiteral("TARGET"));
        QCOMPARE(ok[0].title(), QStringLiteral("vda"));  // no model on virtio
        QCOMPARE(ok[0].detail(), QStringLiteral("69 GB · /dev/vda"));
        QCOMPARE(ok[1].path, QStringLiteral("/dev/nvme0n1"));
        QCOMPARE(ok[1].title(), QStringLiteral("Samsung SSD 980 PRO 500GB"));
        QCOMPARE(ok[1].detail(), QStringLiteral("512 GB · NVMe · /dev/nvme0n1"));
    }

    void disksLiveUsbBySource() {
        // Old lsblk: strings for numbers and flags, `mountpoint` not `mountpoints`.
        const auto all = disks::parse(fixture("lsblk-usb-live.json").toUtf8(), "/dev/sda1", QString());
        QCOMPARE(all.size(), 2);
        QCOMPARE(byName(all, "sda")->exclusion, Exclusion::LiveMedium);
        QVERIFY(byName(all, "sda")->removable);
        QCOMPARE(byName(all, "sda")->model, QStringLiteral("Ultra Fit"));  // trimmed
        QCOMPARE(byName(all, "sdb")->exclusion, Exclusion::None);
        QCOMPARE(byName(all, "sdb")->detail(), QStringLiteral("1 TB · SATA · /dev/sdb"));
    }

    void disksLiveUsbByLabelAlone() {
        // `rd.live.ram` copies the medium and unmounts it: no source, the label
        // still marks it. Unmounted, it would otherwise be offered.
        QByteArray json = fixture("lsblk-usb-live.json").toUtf8();
        json.replace("\"/run/initramfs/live\"", "null");
        const auto all = disks::parse(json, QString(), "Mauvely-OS-Installer");
        QCOMPARE(byName(all, "sda")->exclusion, Exclusion::LiveMedium);
        QCOMPARE(disks::eligible(all).size(), 1);
    }

    void disksGarbage() {
        QString err;
        QVERIFY(disks::parse("lsblk: unknown column", "", "", &err).isEmpty());
        QVERIFY(!err.isEmpty());
        err.clear();
        QVERIFY(disks::parse("{\"blockdevices\": []}", "", "", &err).isEmpty());
        QVERIFY(err.isEmpty());
    }

    void humanSize_data() {
        QTest::addColumn<qint64>("bytes");
        QTest::addColumn<QString>("text");
        QTest::newRow("8 GB stick") << qint64(8004304896) << "8 GB";
        QTest::newRow("2.9 GB") << qint64(2876637184) << "2.9 GB";
        QTest::newRow("64 GiB") << qint64(68719476736) << "69 GB";
        QTest::newRow("1 TB") << qint64(1000204886016) << "1 TB";
        QTest::newRow("4 TB") << qint64(4000787030016) << "4 TB";
        QTest::newRow("2.5 TB") << qint64(2500000000000) << "2.5 TB";
        QTest::newRow("12 TB") << qint64(12000138625024) << "12 TB";
    }
    void humanSize() {
        QFETCH(qint64, bytes);
        QFETCH(QString, text);
        QCOMPARE(disks::humanSize(bytes), text);
    }

    void hostListsThroughTheRunner() {
        FakeRunner fake;
        const auto all = host::listDisks(fake, Config{});
        QCOMPARE(disks::eligible(all).size(), 2);
        QCOMPARE(fake.calls().first().line(), QStringLiteral("findmnt -n -o SOURCE /run/initramfs/live"));

        fake.script["lsblk"] = [](const QStringList &) { return RunResult{1, "lsblk: bad usage"}; };
        QString err;
        QVERIFY(host::listDisks(fake, Config{}, &err).isEmpty());
        QVERIFY(err.contains("bad usage"));
    }

    // ── the token and the server ────────────────────────────────────────────

    void token_data() {
        QTest::addColumn<QString>("token");
        QTest::addColumn<bool>("ok");
        QTest::newRow("well formed") << kToken << true;
        QTest::newRow("all A") << QStringLiteral("mvd_") + QString(43, 'A') << true;
        QTest::newRow("empty") << QString() << false;
        QTest::newRow("no prefix") << kToken.mid(4) << false;
        QTest::newRow("other prefix") << QStringLiteral("mvx_") + kToken.mid(4) << false;
        QTest::newRow("upper prefix") << QStringLiteral("MVD_") + kToken.mid(4) << false;
        QTest::newRow("one short") << kToken.chopped(1) << false;
        QTest::newRow("one long") << kToken + QLatin1Char('A') << false;
        QTest::newRow("padded") << kToken.chopped(1) + QLatin1Char('=') << false;
        QTest::newRow("standard base64") << kToken.chopped(2) + QStringLiteral("+/") << false;
        QTest::newRow("space inside") << kToken.left(20) + QLatin1Char(' ') + kToken.mid(21) << false;
        // The 43rd character holds 4 bits and 2 zero bits; a non-zero tail
        // does not re-encode to itself, so a typo there is caught too.
        QTest::newRow("bad last char") << kToken.chopped(1) + QLatin1Char('R') << false;
    }
    void token() {
        QFETCH(QString, token);
        QFETCH(bool, ok);
        QCOMPARE(enrolment::isTokenWellFormed(token), ok);
    }

    void tokenTrimmed() {
        QCOMPARE(enrolment::normaliseToken(QStringLiteral("  ") + kToken + QStringLiteral("\n")), kToken);
        QVERIFY(enrolment::isTokenWellFormed(enrolment::normaliseToken(QLatin1Char(' ') + kToken)));
    }

    void cmdline_data() {
        QTest::addColumn<QString>("line");
        QTest::addColumn<QString>("server");
        const QString base = QStringLiteral(
            "BOOT_IMAGE=/images/pxeboot/vmlinuz root=live:CDLABEL=Mauvely-OS-Installer "
            "rd.live.image rd.live.overlay.overlayfs=1 console=tty0 console=ttyS0,115200 selinux=0");
        QTest::newRow("none") << base << "https://devices.mauvely.com";
        QTest::newRow("local") << base + " mauvely.server=http://10.0.2.2:8798"
                               << "http://10.0.2.2:8798";
        QTest::newRow("trailing slash") << "mauvely.server=https://devices.example.org/ quiet"
                                        << "https://devices.example.org";
        QTest::newRow("first of line") << "mauvely.server=https://a.example quiet" << "https://a.example";
        QTest::newRow("last wins") << "mauvely.server=https://a.example mauvely.server=https://b.example"
                                   << "https://b.example";
        QTest::newRow("quoted") << "x=1 \"mauvely.server=https://q.example\" y" << "https://q.example";
        QTest::newRow("not a url") << "mauvely.server=devices.example.org" << "https://devices.mauvely.com";
        QTest::newRow("ftp") << "mauvely.server=ftp://devices.example.org" << "https://devices.mauvely.com";
        QTest::newRow("empty") << "mauvely.server= quiet" << "https://devices.mauvely.com";
        QTest::newRow("bare flag") << "mauvely.server quiet" << "https://devices.mauvely.com";
        QTest::newRow("prefix only") << "xmauvely.server=https://x.example" << "https://devices.mauvely.com";
        QTest::newRow("with query") << "mauvely.server=https://x.example/?a=b" << "https://devices.mauvely.com";
        QTest::newRow("with user") << "mauvely.server=https://u:p@x.example" << "https://devices.mauvely.com";
    }
    void cmdline() {
        QFETCH(QString, line);
        QFETCH(QString, server);
        QCOMPARE(enrolment::initialServer(line), server);
    }

    void cmdlineFlags() {
        const QString line = QStringLiteral("root=live:CDLABEL=X rd.live.image rd.live.overlay.overlayfs=1");
        QVERIFY(cmdline::hasFlag(line, "rd.live.image"));
        QVERIFY(!cmdline::hasFlag(line, "rd.live"));
        QVERIFY(!cmdline::hasFlag(line, "rd.live.overlay.overlayfs"));  // has a value, not a flag
        QCOMPARE(cmdline::value(line, "root"), QStringLiteral("live:CDLABEL=X"));
        QVERIFY(cmdline::value(line, "quiet").isNull());
        QCOMPARE(cmdline::split("a \"b c\" d"), (QStringList{"a", "b c", "d"}));
    }

    void serverUrl_data() {
        QTest::addColumn<QString>("url");
        QTest::addColumn<bool>("ok");
        QTest::newRow("https") << "https://devices.mauvely.com" << true;
        QTest::newRow("http local") << "http://10.0.2.2:8798" << true;
        QTest::newRow("path") << "https://example.org/devices" << true;
        QTest::newRow("padded") << "  https://example.org  " << true;
        QTest::newRow("no scheme") << "devices.mauvely.com" << false;
        QTest::newRow("no host") << "https://" << false;
        QTest::newRow("file") << "file:///etc/passwd" << false;
        QTest::newRow("space") << "https://exa mple.org" << false;
        QTest::newRow("fragment") << "https://example.org/#x" << false;
        QTest::newRow("empty") << "" << false;
    }
    void serverUrl() {
        QFETCH(QString, url);
        QFETCH(bool, ok);
        QCOMPARE(enrolment::isServerUrl(url), ok);
    }

    void liveSystemGuard() {
        QTemporaryDir dir;
        Config c;
        c.procCmdline = dir.filePath("cmdline");
        QVERIFY(!host::isLiveSystem(c));  // unreadable: refuse
        QVERIFY(writeFile(c.procCmdline, "BOOT_IMAGE=(hd0,gpt2)/vmlinuz root=UUID=1 rw quiet\n"));
        QVERIFY(!host::isLiveSystem(c));
        QVERIFY(writeFile(c.procCmdline, "root=live:CDLABEL=Mauvely-OS-Installer rd.live.image selinux=0\n"));
        QVERIFY(host::isLiveSystem(c));
    }

    void tpm() {
        QTemporaryDir dir;
        Config c;
        c.sysClassTpm = dir.filePath("tpm");
        QVERIFY(!host::hasTpm2(c));
        QVERIFY(writeFile(c.sysClassTpm + "/tpm0/tpm_version_major", "1\n"));
        QVERIFY(!host::hasTpm2(c));
        QVERIFY(writeFile(c.sysClassTpm + "/tpm1/tpm_version_major", "2\n"));
        QVERIFY(host::hasTpm2(c));
    }

    // ── copy progress ───────────────────────────────────────────────────────

    void copyTotal_data() {
        QTest::addColumn<QString>("line");
        QTest::addColumn<qint64>("bytes");
        // The line as P6's run printed it, and the other units glib's
        // g_format_size can give it.
        QTest::newRow("the real line") << "layers already present: 0; layers needed: 266 (7.5 GB)" << qint64(7500000000);
        QTest::newRow("with a timestamp") << "02:36:59 layers already present: 0; layers needed: 266 (7.5 GB)" << qint64(7500000000);
        QTest::newRow("some present") << "layers already present: 12; layers needed: 254 (6.1 GB)" << qint64(6100000000);
        QTest::newRow("MB") << "layers already present: 0; layers needed: 3 (512.3 MB)" << qint64(512300000);
        QTest::newRow("kB") << "layers already present: 0; layers needed: 1 (980 kB)" << qint64(980000);
        QTest::newRow("TB") << "layers already present: 0; layers needed: 9 (1.4 TB)" << qint64(1400000000000);
        QTest::newRow("GiB") << "layers already present: 0; layers needed: 266 (7.5 GiB)" << qint64(8053063680);
        QTest::newRow("MiB") << "layers already present: 0; layers needed: 3 (512 MiB)" << qint64(536870912);
        QTest::newRow("bytes") << "layers already present: 0; layers needed: 1 (999 bytes)" << qint64(999);
        QTest::newRow("one byte") << "layers already present: 0; layers needed: 1 (1 byte)" << qint64(1);
        QTest::newRow("nothing to fetch") << "layers already present: 266; layers needed: 0 (0 bytes)" << qint64(0);
        QTest::newRow("another line") << "Installing image: docker://ghcr.io/mauvely/os:stable" << qint64(0);
        QTest::newRow("no size") << "layers already present: 0; layers needed: 266" << qint64(0);
        QTest::newRow("a unit it does not print") << "layers already present: 0; layers needed: 266 (7.5 furlongs)" << qint64(0);
        QTest::newRow("empty") << "" << qint64(0);
    }
    void copyTotal() {
        QFETCH(QString, line);
        QFETCH(qint64, bytes);
        QCOMPARE(copyprogress::totalFromLine(line), bytes);
    }

    void copyStat_data() {
        QTest::addColumn<QByteArray>("stat");
        QTest::addColumn<qint64>("sectors");
        // Field 7 is sectors written; this is a real line from an NVMe disk.
        QTest::newRow("real") << QByteArray(" 3680926   199961 520197888  2068253 30685184  1721908 1549671740  3209514        0  1298819  5521827        0        0        0        0  1558552   244058\n")
                              << qint64(1549671740);
        QTest::newRow("old kernel, eleven fields") << QByteArray("10 0 80 5 20 0 160 9 0 7 14\n") << qint64(160);
        QTest::newRow("nothing written") << QByteArray("10 0 80 5 0 0 0 0 0 7 14\n") << qint64(0);
        QTest::newRow("too few fields") << QByteArray("10 0 80 5 20 0\n") << qint64(-1);
        QTest::newRow("not numbers") << QByteArray("a b c d e f g h\n") << qint64(-1);
        QTest::newRow("negative") << QByteArray("1 2 3 4 5 6 -7 8\n") << qint64(-1);
        QTest::newRow("empty") << QByteArray() << qint64(-1);
    }
    void copyStat() {
        QFETCH(QByteArray, stat);
        QFETCH(qint64, sectors);
        QCOMPARE(copyprogress::sectorsWritten(stat), sectors);
    }

    void copySysfsName() {
        QCOMPARE(copyprogress::sysfsName("/dev/vda"), QStringLiteral("vda"));
        QCOMPARE(copyprogress::sysfsName("/dev/nvme0n1"), QStringLiteral("nvme0n1"));
        QCOMPARE(copyprogress::sysfsName("/dev/mmcblk0"), QStringLiteral("mmcblk0"));
        QCOMPARE(copyprogress::sysfsName("/dev/cciss/c0d0"), QStringLiteral("cciss!c0d0"));  // sysfs has no slash in a name
        QVERIFY(copyprogress::sysfsName("vda").isEmpty());
        QVERIFY(copyprogress::sysfsName("/dev/").isEmpty());
        QVERIFY(copyprogress::sysfsName(QString()).isEmpty());
    }

    void copyReadsTheDisksCounter() {
        QTemporaryDir dir;
        Config c;
        c.sysBlock = dir.filePath("block");
        QCOMPARE(host::sectorsWritten(c, "/dev/vda"), qint64(-1));  // no such disk
        QVERIFY(writeFile(c.sysBlock + "/vda/stat", "10 0 80 5 20 0 2048 9 0 7 14\n"));
        QVERIFY(writeFile(c.sysBlock + "/nvme0n1/stat", "10 0 80 5 20 0 4096 9 0 7 14\n"));
        QCOMPARE(host::sectorsWritten(c, "/dev/vda"), qint64(2048));
        QCOMPARE(host::sectorsWritten(c, "/dev/nvme0n1"), qint64(4096));
        QCOMPARE(host::sectorsWritten(c, "vda"), qint64(-1));
        QVERIFY(writeFile(c.sysBlock + "/vda/stat", "garbage\n"));
        QCOMPARE(host::sectorsWritten(c, "/dev/vda"), qint64(-1));
    }

    void meterIsTheDeltaFromTheFirstReading() {
        copyprogress::Meter m;
        m.setTotal(7500000000);
        m.sample(1000000);  // the baseline: what the disk had counted before the copy
        QCOMPARE(m.written(), qint64(0));
        QCOMPARE(m.fraction(), 0.0);
        QCOMPARE(m.text(), QStringLiteral("0.0 of 7.5 GB"));
        // 3.1 GB in 512-byte sectors, however large the disk's own are.
        m.sample(1000000 + 6054687);
        QCOMPARE(m.written(), qint64(6054687) * 512);
        QCOMPARE(m.text(), QStringLiteral("3.1 of 7.5 GB"));
        QVERIFY(qAbs(m.fraction() - 3.1 / 7.5) < 0.001);
    }

    void meterStopsShortOfFull() {
        copyprogress::Meter m;
        m.setTotal(7500000000);
        m.sample(0);
        m.sample(7500000000 / 512);  // exactly the total
        QCOMPARE(m.fraction(), 0.99);
        QCOMPARE(m.text(), QStringLiteral("7.5 of 7.5 GB"));
        m.sample(9000000000 / 512);  // more than the image, as with metadata
        QCOMPARE(m.fraction(), 0.99);
        QCOMPARE(m.text(), QStringLiteral("7.5 of 7.5 GB"));  // never "9.0 of 7.5"
        QVERIFY(m.written() > m.total());
    }

    void meterNeverGoesBack() {
        copyprogress::Meter m;
        m.setTotal(4000000000);
        m.sample(5000);
        m.sample(5000 + 1000000);
        const qint64 before = m.written();
        m.sample(5000 + 400000);  // a counter that fell back
        QCOMPARE(m.written(), before);
        m.sample(100);  // below the baseline
        QCOMPARE(m.written(), before);
        m.sample(-1);  // unreadable
        QCOMPARE(m.written(), before);
    }

    void meterWithoutATotalShowsWhatIsWritten() {
        copyprogress::Meter m;
        m.sample(0);
        // Partitioning and the LUKS header: too little to say anything about.
        m.sample(60000);  // 30 MB
        QCOMPARE(m.fraction(), -1.0);
        QVERIFY(m.text().isEmpty());
        m.sample(400000);  // 205 MB
        QCOMPARE(m.fraction(), -1.0);
        QCOMPARE(m.text(), QStringLiteral("205 MB written"));
        m.sample(6054687);
        QCOMPARE(m.fraction(), -1.0);
        QCOMPARE(m.text(), QStringLiteral("3.1 GB written"));
    }

    void meterWithoutACounterSaysNothing() {
        copyprogress::Meter m;
        m.setTotal(7500000000);  // the total alone is bootc's own line again
        m.sample(-1);
        m.sample(-1);
        QCOMPARE(m.fraction(), -1.0);
        QVERIFY(m.text().isEmpty());
        m.setTotal(0);
        QVERIFY(m.text().isEmpty());
    }

    void meterBaselineWaitsForAReading() {
        copyprogress::Meter m;
        m.setTotal(2000000000);
        m.sample(-1);  // unreadable at the start
        m.sample(900000);  // the first it can read is the baseline
        QCOMPARE(m.written(), qint64(0));
        m.sample(900000 + 1953125);  // 1 GB
        QCOMPARE(m.text(), QStringLiteral("1.0 of 2.0 GB"));
    }

    void meterSmallTotalIsInMegabytes() {
        copyprogress::Meter m;
        m.setTotal(512000000);
        m.sample(0);
        m.sample(250000000 / 512);
        QCOMPARE(m.text(), QStringLiteral("250 of 512 MB"));
    }

    // ── bootc ───────────────────────────────────────────────────────────────

    void installArgs() {
        QCOMPARE(bootc::installArgs("localhost/mauvely-os:p6-test", "/dev/vda").join(' '),
                 QStringLiteral("install to-disk --source-imgref containers-storage:localhost/mauvely-os:p6-test "
                                "--wipe --block-setup tpm2-luks --filesystem xfs "
                                "--target-imgref ghcr.io/mauvely/os:stable --skip-fetch-check /dev/vda"));
    }

    void classify_data() {
        QTest::addColumn<int>("exitCode");
        QTest::addColumn<QString>("output");
        QTest::addColumn<int>("outcome");
        const QString leaked = fixture("bootc-leaked-mount.log");
        QTest::newRow("clean") << 0 << QStringLiteral("Closing root LUKS device\nInstallation complete!\n")
                               << int(bootc::Outcome::Succeeded);
        QTest::newRow("leaked mount") << 1 << leaked << int(bootc::Outcome::LeakedMount);
        QTest::newRow("leaked, trailing blank") << 1 << leaked + "\n\n" << int(bootc::Outcome::LeakedMount);
        QTest::newRow("tpm") << 1 << fixture("bootc-tpm-failure.log") << int(bootc::Outcome::Failed);
        // The known line, but not last: something failed after it.
        QTest::newRow("not last") << 1 << leaked + "error: something else\n" << int(bootc::Outcome::Failed);
        // Mentioned, not failed.
        QTest::newRow("progress line") << 1 << QStringLiteral("Closing root LUKS device\nerror: Installing to disk: "
                                                              "Task Unmounting filesystems failed\n")
                                       << int(bootc::Outcome::Failed);
        QTest::newRow("killed") << -1 << leaked << int(bootc::Outcome::Failed);
        QTest::newRow("no output") << 1 << QString() << int(bootc::Outcome::Failed);
    }
    void classify() {
        QFETCH(int, exitCode);
        QFETCH(QString, output);
        QFETCH(int, outcome);
        QCOMPARE(int(bootc::classify(exitCode, output)), outcome);
    }

    void workaround() {
        const auto cmds = bootc::leakedMountWorkaround();
        QCOMPARE(cmds.size(), 2);
        QCOMPARE(cmds[0].join(' '), QStringLiteral("umount /run/bootc/storage"));
        QCOMPARE(cmds[1].join(' '), QStringLiteral("cryptsetup close root"));
    }

    void payload() {
        QString err;
        QCOMPARE(bootc::payloadFromPodmanImages(fixture("podman-images.json").toUtf8(), &err),
                 QStringLiteral("localhost/mauvely-os:p6-test"));
        QVERIFY(bootc::payloadFromPodmanImages("[]", &err).isEmpty());
        QCOMPARE(err, QStringLiteral("The install medium carries no OS image."));

        const QByteArray two = "[{\"Names\":[\"localhost/a:1\"]},{\"Names\":[\"localhost/b:1\"]}]";
        QVERIFY(bootc::payloadFromPodmanImages(two, &err).isEmpty());
        QVERIFY(err.contains("more than one"));

        QVERIFY(bootc::payloadFromPodmanImages("not json", &err).isEmpty());
        QVERIFY(!err.isEmpty());
    }

    // ── LUKS ────────────────────────────────────────────────────────────────

    void luksPartition() {
        QString err;
        QCOMPARE(luks::partition(fixture("lsblk-luks.json").toUtf8(), &err), QStringLiteral("/dev/vda4"));
        QByteArray none = fixture("lsblk-luks.json").toUtf8();
        none.replace("crypto_LUKS", "xfs");
        QVERIFY(luks::partition(none, &err).isEmpty());
        QVERIFY(!err.isEmpty());
        QCOMPARE(luks::addRecoveryKeyArgs("/dev/vda4").join(' '),
                 QStringLiteral("--unlock-tpm2-device=auto --recovery-key /dev/vda4"));
    }

    void recoveryKey() {
        const QString key = QStringLiteral("fvhkcbjd-lnrtigku-bhefjrtv-vncbtgeh-ddilrkvu-ifnhbutr-ecdjgrtn-likdheuf");
        QCOMPARE(luks::recoveryKey(fixture("cryptenroll-recovery.txt")), key);
        QCOMPARE(luks::recoveryKey(key + "\n"), key);  // no emoji, no prose
        QVERIFY(luks::recoveryKey("Failed to unlock: No such device").isEmpty());
        QVERIFY(luks::recoveryKey(key.chopped(1)).isEmpty());                   // seven and a bit
        QVERIFY(luks::recoveryKey(key.left(70) + "a").isEmpty());              // 'a' is not modhex
        QVERIFY(luks::recoveryKey(key + "\n" + QString(key).replace('f', 'g')).isEmpty());  // two keys
        QVERIFY(luks::recoveryKey(key + "-cbdefghi").isEmpty());                // nine groups
    }

    // ── the target ──────────────────────────────────────────────────────────

    void deployment() {
        QTemporaryDir root;
        QString err;
        QVERIFY(target::findDeployment(root.path(), &err).isEmpty());
        QVERIFY(!err.isEmpty());

        const QString dep = root.filePath("ostree/deploy/default/deploy/3f2a9c.0");
        QVERIFY(QDir().mkpath(dep + "/etc"));
        QCOMPARE(target::findDeployment(root.path(), &err), dep);

        // A .origin file sits beside the deployment; only directories count.
        QVERIFY(writeFile(root.filePath("ostree/deploy/default/deploy/3f2a9c.0.origin"), "[origin]\n"));
        QCOMPARE(target::findDeployment(root.path(), &err), dep);

        QVERIFY(QDir().mkpath(root.filePath("ostree/deploy/default/deploy/77aa01.0/etc")));
        QVERIFY(target::findDeployment(root.path(), &err).isEmpty());  // two: which?
    }

    void enrolmentFiles() {
        QTemporaryDir root;
        const QString dep = root.filePath("deploy");
        QVERIFY(QDir().mkpath(dep + "/etc"));
        QString err;
        QVERIFY2(target::writeEnrolment(dep, "http://10.0.2.2:8798", kToken, &err), qPrintable(err));

        QFile server(dep + "/etc/mauvely/server"), enrol(dep + "/etc/mauvely/enrol");
        QVERIFY(server.open(QIODevice::ReadOnly) && enrol.open(QIODevice::ReadOnly));
        QCOMPARE(server.readAll(), QByteArray("http://10.0.2.2:8798\n"));
        QCOMPARE(enrol.readAll(), kToken.toLatin1() + '\n');
        QCOMPARE(modeOf(dep + "/etc/mauvely"), 0755);
        QCOMPARE(modeOf(dep + "/etc/mauvely/server"), 0644);
        QCOMPARE(modeOf(dep + "/etc/mauvely/enrol"), 0600);

        // Again over the top, from a looser mode: still 0600, still one token.
        QVERIFY(QFile::setPermissions(dep + "/etc/mauvely/enrol", QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                                                    QFileDevice::ReadOther));
        QVERIFY(target::writeEnrolment(dep, "https://devices.mauvely.com", kToken, &err));
        QCOMPARE(modeOf(dep + "/etc/mauvely/enrol"), 0600);
        QCOMPARE(QFileInfo(dep + "/etc/mauvely/enrol").size(), qint64(kToken.size() + 1));
    }

    void enrolmentRefusesASymlink() {
        QTemporaryDir root;
        const QString dep = root.filePath("deploy");
        QVERIFY(QDir().mkpath(dep + "/etc/mauvely"));
        QVERIFY(QFile::link(root.filePath("elsewhere"), dep + "/etc/mauvely/enrol"));
        QString err;
        QVERIFY(!target::writeEnrolment(dep, "https://devices.mauvely.com", kToken, &err));
        QVERIFY(!QFileInfo::exists(root.filePath("elsewhere")));
    }

    void selinux() {
        QTemporaryDir root;
        const QString dep = root.filePath("deploy");
        QCOMPARE(target::selinuxType(dep), QStringLiteral("targeted"));  // no config: the default
        QVERIFY(writeFile(dep + "/etc/selinux/config", "# comment\nSELINUX=enforcing\nSELINUXTYPE=mls\n"));
        QCOMPARE(target::selinuxType(dep), QStringLiteral("mls"));
        QCOMPARE(target::setfilesArgs(dep),
                 (QStringList{"-F", "-r", dep, dep + "/etc/selinux/mls/contexts/files/file_contexts",
                              dep + "/etc/mauvely"}));
    }
};

QTEST_GUILESS_MAIN(TestLogic)
#include "tst_logic.moc"
