// ─────────────────────────────────────────────────────────────────────────────
//  tst_flow — the real window, clicked through, against a scripted runner.
//
//  Runs offscreen. With INSTALLER_SHOTS=<dir> it also saves each screen as a
//  PNG, which is how the pages are reviewed without a live system.
// ─────────────────────────────────────────────────────────────────────────────

#include "core/installer.h"
#include "core/theme.h"
#include "fakerunner.h"
#include "ui/pages.h"
#include "ui/style.h"
#include "ui/window.h"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSemaphore>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QtTest>

namespace {

const QString kToken = QStringLiteral("mvd_q3Zt9Xw0yK8Lr2Vb5Nc7Hd1Jf4Gs6Ap0Me3Ui8Oo_-Q");
const QString kKey = QStringLiteral("fvhkcbjd-lnrtigku-bhefjrtv-vncbtgeh-ddilrkvu-ifnhbutr-ecdjgrtn-likdheuf");

bool writeFile(const QString &path, const QByteArray &bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size();
}

}  // namespace

class TestFlow : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    Config m_config;
    FakeRunner m_fake;
    std::unique_ptr<ui::Window> m_window;

    QWidget *page() const { return m_window->findChild<QStackedWidget *>()->currentWidget(); }
    QString pageName() const { return page()->objectName(); }

    QPushButton *button(const QString &text) const {
        for (auto *b : page()->findChildren<QPushButton *>())
            if (b->text() == text) return b;
        return nullptr;
    }

    void click(const QString &text) {
        QPushButton *b = button(text);
        QVERIFY2(b, qPrintable("no button " + text + " on " + pageName()));
        QVERIFY2(b->isEnabled(), qPrintable(text + " is disabled on " + pageName()));
        QTest::mouseClick(b, Qt::LeftButton);
    }

    void shot(const QString &name) {
        const QString dir = qEnvironmentVariable("INSTALLER_SHOTS");
        if (dir.isEmpty()) return;
        QDir().mkpath(dir);
        QCoreApplication::processEvents();
        QVERIFY(m_window->grab().save(dir + QLatin1Char('/') + name + QStringLiteral(".png")));
    }

    void newWindow(bool tpm = true) {
        m_window.reset();
        QDir(m_dir.filePath("tpm")).removeRecursively();
        if (tpm) QVERIFY(writeFile(m_config.sysClassTpm + "/tpm0/tpm_version_major", "2\n"));
        m_window = std::make_unique<ui::Window>(m_fake, m_config);
        m_window->resize(1280, 800);
        m_window->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_window.get()));
    }

    void toDiskPage() {
        click("Start");
        QCOMPARE(pageName(), QStringLiteral("enrolment"));
        QTest::keyClicks(page()->findChild<QLineEdit *>("token"), kToken);
        click("Continue");
        QCOMPARE(pageName(), QStringLiteral("disk"));
    }

    void chooseVda() {
        auto *vda = page()->findChild<QAbstractButton *>("disk:/dev/vda");
        QVERIFY(vda);
        QTest::mouseClick(vda, Qt::LeftButton);
        QVERIFY(vda->isChecked());
    }

private slots:
    void initTestCase() {
        style::loadFonts();
        Theme::instance().apply();
        qApp->setStyleSheet(style::sheet());

        m_config.procCmdline = m_dir.filePath("cmdline");
        m_config.sysClassTpm = m_dir.filePath("tpm");
        m_config.logPath = m_dir.filePath("log/mauvely-installer.log");
        m_config.mountPoint = m_dir.filePath("target");
        QVERIFY(writeFile(m_config.procCmdline,
                          "root=live:CDLABEL=Mauvely-OS-Installer rd.live.image rd.live.overlay.overlayfs=1 "
                          "selinux=0 mauvely.server=http://10.0.2.2:8798\n"));
        QVERIFY(QDir().mkpath(m_config.mountPoint + "/ostree/deploy/default/deploy/3f2a9c.0/etc"));
    }

    void init() {
        m_fake.reset();
        m_fake.before = nullptr;
    }

    void noTpmCannotStart() {
        newWindow(/*tpm*/ false);
        QCOMPARE(pageName(), QStringLiteral("welcome"));
        QVERIFY(!button("Start")->isEnabled());
        shot("0-welcome-no-tpm");
    }

    void happyPath() {
        newWindow();
        QCOMPARE(pageName(), QStringLiteral("welcome"));
        shot("1-welcome");

        click("Start");
        QCOMPARE(pageName(), QStringLiteral("enrolment"));
        auto *server = page()->findChild<QLineEdit *>("server");
        auto *token = page()->findChild<QLineEdit *>("token");
        QCOMPARE(server->text(), QStringLiteral("http://10.0.2.2:8798"));  // from the kernel argument
        QVERIFY(token->hasFocus());
        shot("2-enrolment");

        // A bad token stays on the page and says why.
        QTest::keyClicks(token, kToken.chopped(1));
        click("Continue");
        QCOMPARE(pageName(), QStringLiteral("enrolment"));
        shot("2b-enrolment-error");
        token->clear();
        QTest::keyClicks(token, kToken);
        click("Continue");

        // The disk page: lsblk ran, two disks are offered, none is chosen.
        QCOMPARE(pageName(), QStringLiteral("disk"));
        const auto disks = page()->findChildren<ui::DiskButton *>();
        QCOMPARE(disks.size(), 2);
        for (auto *d : disks) QVERIFY(!d->isChecked());
        QVERIFY(!button("Continue")->isEnabled());
        shot("3-disk");
        chooseVda();
        QVERIFY(button("Continue")->isEnabled());
        shot("3b-disk-chosen");
        click("Continue");

        // Confirm names the disk. Nothing has been written yet.
        QCOMPARE(pageName(), QStringLiteral("confirm"));
        const QStringList texts = [this] {
            QStringList out;
            for (auto *l : page()->findChildren<QLabel *>()) out << l->text();
            return out;
        }();
        QVERIFY(texts.contains("Erase vda?"));
        QVERIFY(texts.contains("69 GB · /dev/vda"));
        for (const QString &l : m_fake.lines()) QVERIFY2(!l.startsWith("bootc"), qPrintable(l));
        shot("4-confirm");

        // Hold bootc in the middle, to see the install page.
        QSemaphore started, release;
        m_fake.before = [&](const QString &program, const Runner::LineFn &onLine) {
            if (program != QLatin1String("bootc")) return;
            if (onLine) onLine(QStringLiteral("Installing image: docker://registry.invalid/mauvely/os:stable"));
            started.release();
            release.acquire();
        };
        click("Erase and install");
        QCOMPARE(pageName(), QStringLiteral("install"));
        QVERIFY(started.tryAcquire(1, 5000));
        QTest::qWait(100);
        shot("5-install");
        release.release();

        QTRY_COMPARE_WITH_TIMEOUT(pageName(), QStringLiteral("recovery"), 5000);
        auto *key = page()->findChild<QLabel *>("key");
        QCOMPARE(key->text().remove('\n'), kKey);
        QVERIFY(!button("Finish")->isEnabled());
        // The token left the window when it reached the disk.
        QVERIFY(m_window->findChild<QLineEdit *>("token")->text().isEmpty());
        shot("6-recovery");

        page()->findChild<QCheckBox *>("written")->click();
        QVERIFY(button("Finish")->isEnabled());
        click("Finish");
        QCOMPARE(pageName(), QStringLiteral("done"));
        // The key left the window too.
        QVERIFY(m_window->findChild<QLabel *>("key")->text().isEmpty());
        shot("7-done");

        QVERIFY(!m_fake.lines().contains("systemctl reboot"));
        click("Restart");
        QCOMPARE(m_fake.lines().last(), QStringLiteral("systemctl reboot"));
    }

    void failedThenAgain() {
        m_fake.script["bootc"] = [](const QStringList &) {
            return RunResult{1, fixture(QStringLiteral("bootc-tpm-failure.log"))};
        };
        newWindow();
        toDiskPage();
        chooseVda();
        click("Continue");
        click("Erase and install");
        QTRY_COMPARE_WITH_TIMEOUT(pageName(), QStringLiteral("failed"), 5000);
        shot("8-failed");

        // Start again keeps the server and token, and asks for the disk again.
        click("Start again");
        QCOMPARE(pageName(), QStringLiteral("disk"));
        QVERIFY(!button("Continue")->isEnabled());
        QCOMPARE(m_window->findChild<QLineEdit *>("token")->text(), kToken);
    }

    void confirmIsOneClick() {
        QSemaphore release;
        m_fake.before = [&](const QString &program, const Runner::LineFn &) {
            if (program == QLatin1String("bootc")) release.acquire();
        };
        newWindow();
        toDiskPage();
        chooseVda();
        click("Continue");
        QPushButton *erase = button("Erase and install");
        QTest::mouseClick(erase, Qt::LeftButton);
        QVERIFY(!erase->isEnabled());
        release.release();
        QTRY_COMPARE_WITH_TIMEOUT(pageName(), QStringLiteral("recovery"), 5000);
        int bootcRuns = 0;
        for (const QString &l : m_fake.lines()) bootcRuns += l.startsWith("bootc");
        QCOMPARE(bootcRuns, 1);
    }

    void noDisks() {
        m_fake.script["lsblk"] = [](const QStringList &) {
            return RunResult{0, fixture(QStringLiteral("lsblk-usb-live.json")).replace("\"sdb\"", "\"zram1\"")};
        };
        newWindow();
        toDiskPage();
        QCOMPARE(page()->findChildren<ui::DiskButton *>().size(), 0);
        QVERIFY(!button("Continue")->isEnabled());
        shot("3c-disk-none");
    }

    void cleanupTestCase() { m_window.reset(); }
};

QTEST_MAIN(TestFlow)
#include "tst_flow.moc"
