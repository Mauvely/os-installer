#include "ui/window.h"

#include "core/enrolment.h"
#include "core/host.h"
#include "core/installer.h"
#include "core/runner.h"
#include "core/theme.h"
#include "core/tokens.h"
#include "ui/pages.h"
#include "ui/style.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace ui {

Window::Window(Runner &runner, const Config &config, QWidget *parent)
    : QWidget(parent),
      m_runner(runner),
      m_config(config),
      m_installer(new Installer(runner, config, this)),
      m_stack(new QStackedWidget(this)) {
    setObjectName(QStringLiteral("installer"));
    setAttribute(Qt::WA_StyledBackground);
    setWindowTitle(QStringLiteral("Mauvely OS installer"));

    // The wordmark: "mauvely" and a teal full stop, as everywhere else.
    const Theme &t = Theme::instance();
    auto *mark = new QLabel(this);
    mark->setTextFormat(Qt::RichText);
    mark->setFont(style::display(tokens::type::kXl));
    mark->setText(QStringLiteral("<span style='color:%1'>mauvely</span><span style='color:%2'>.</span>")
                      .arg(t.text().name(), t.signal().name()));

    auto *bar = new QHBoxLayout;
    bar->setContentsMargins(tokens::space::k32, tokens::space::k24, tokens::space::k32, 0);
    bar->addWidget(mark);
    bar->addStretch(1);

    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    v->addLayout(bar);
    v->addWidget(m_stack, 1);

    m_welcome = new WelcomePage(host::hasTpm2(config), m_stack);
    m_enrolment = new EnrolmentPage(enrolment::initialServer(host::kernelCmdline(config)), m_stack);
    m_disk = new DiskPage(m_stack);
    m_confirm = new ConfirmPage(m_stack);
    m_install = new InstallPage(m_stack);
    m_recovery = new RecoveryPage(m_stack);
    m_done = new DonePage(m_stack);
    m_failed = new FailedPage(config.logPath, m_stack);
    for (QWidget *p : {static_cast<QWidget *>(m_welcome), static_cast<QWidget *>(m_enrolment),
                       static_cast<QWidget *>(m_disk), static_cast<QWidget *>(m_confirm),
                       static_cast<QWidget *>(m_install), static_cast<QWidget *>(m_recovery),
                       static_cast<QWidget *>(m_done), static_cast<QWidget *>(m_failed)})
        m_stack->addWidget(p);

    connect(m_welcome, &WelcomePage::next, this, [this] { showPage(m_enrolment); });

    connect(m_enrolment, &EnrolmentPage::back, this, [this] { showPage(m_welcome); });
    connect(m_enrolment, &EnrolmentPage::next, this, [this] {
        loadDisks();
        showPage(m_disk);
    });

    connect(m_disk, &DiskPage::back, this, [this] { showPage(m_enrolment); });
    connect(m_disk, &DiskPage::refresh, this, &Window::loadDisks);
    connect(m_disk, &DiskPage::next, this, [this] {
        m_confirm->setDisk(m_disk->selected());
        showPage(m_confirm);
    });

    connect(m_confirm, &ConfirmPage::back, this, [this] { showPage(m_disk); });
    connect(m_confirm, &ConfirmPage::confirmed, this, &Window::install);

    connect(m_installer, &Installer::stepStarted, m_install, &InstallPage::setStep);
    connect(m_installer, &Installer::progress, m_install, &InstallPage::setLine);
    connect(m_installer, &Installer::succeeded, this, [this](const QString &key) {
        m_enrolment->clearToken();  // it is on the disk now, and nowhere else
        m_recovery->setKey(key);
        showPage(m_recovery);
    });
    connect(m_installer, &Installer::failed, this, [this](const QString &message, const QString &tail) {
        m_failed->setFailure(message, tail);
        showPage(m_failed);
    });

    connect(m_failed, &FailedPage::again, this, [this] {
        loadDisks();
        showPage(m_disk);
    });
    connect(m_recovery, &RecoveryPage::finished, this, [this] {
        m_recovery->setKey(QString());
        showPage(m_done);
    });
    connect(m_done, &DonePage::restart, this,
            [this] { m_runner.run(QStringLiteral("systemctl"), {QStringLiteral("reboot")}); });

    showPage(m_welcome);
}

void Window::showPage(QWidget *page) {
    m_stack->setCurrentWidget(page);
    // Focus the token field where there is one, else the primary button, so
    // typing or Enter does the obvious thing.
    QWidget *focus = page->findChild<QWidget *>(QStringLiteral("token"));
    if (!focus) focus = page->findChild<QWidget *>(QStringLiteral("primary"));
    if (focus && focus->isEnabled())
        focus->setFocus();
    else
        page->setFocus();
}

void Window::loadDisks() {
    QString error;
    QList<disks::Disk> eligible;
    for (const disks::Disk &d : host::listDisks(m_runner, m_config, &error))
        if (d.eligible()) eligible << d;
    m_disk->setDisks(eligible, error.isEmpty() ? QString() : QStringLiteral("The disks could not be listed."));
}

void Window::install() {
    const disks::Disk disk = m_disk->selected();
    m_install->reset(disk);
    showPage(m_install);
    m_installer->start({disk, m_enrolment->server(), m_enrolment->token()});
}

}  // namespace ui
