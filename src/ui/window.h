#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  Window — the installer's one full-screen window, and the wiring between its
//  pages and the install sequence.
//
//  Welcome → server and token → disk → confirm → install → recovery key →
//  done → reboot. A failed install offers the disk page again; the server and
//  token stay as they were typed.
// ─────────────────────────────────────────────────────────────────────────────

#include "core/config.h"
#include "core/copyprogress.h"

#include <QWidget>

class Installer;
class QStackedWidget;
class QTimer;
class Runner;

namespace ui {

class ConfirmPage;
class DiskPage;
class DonePage;
class EnrolmentPage;
class FailedPage;
class InstallPage;
class RecoveryPage;
class WelcomePage;

class Window : public QWidget {
    Q_OBJECT
public:
    Window(Runner &runner, const Config &config, QWidget *parent = nullptr);

    Installer *installer() const { return m_installer; }

private:
    void showPage(QWidget *page);
    void loadDisks();
    void install();
    /** The copy's progress: the baseline at its start, then a reading a second. */
    void startCopyMeter();
    void sampleCopy();
    void showCopy();

    Runner &m_runner;
    Config m_config;
    Installer *m_installer;
    QStackedWidget *m_stack;
    copyprogress::Meter m_meter;
    QTimer *m_copyTimer;
    QString m_copyDevice;  // /dev/vda

    WelcomePage *m_welcome;
    EnrolmentPage *m_enrolment;
    DiskPage *m_disk;
    ConfirmPage *m_confirm;
    InstallPage *m_install;
    RecoveryPage *m_recovery;
    DonePage *m_done;
    FailedPage *m_failed;
};

}  // namespace ui
