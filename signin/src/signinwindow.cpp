// ─────────────────────────────────────────────────────────────────────────────
//  SigninWindow — see signinwindow.h.
// ─────────────────────────────────────────────────────────────────────────────

#include "signinwindow.h"

#include "core/brand.h"
#include "core/theme.h"
#include "core/tokens.h"
#include "navpolicy.h"
#include "signinpage.h"
#include "ui/style.h"

#include <QAbstractButton>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QSvgRenderer>
#include <QTimer>
#include <QVBoxLayout>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineView>

namespace {

constexpr int kWidth = 480;
constexpr int kHeight = 640;
constexpr int kHeaderHeight = 64;
constexpr int kBand = 4;
constexpr int kMark = 30;
constexpr int kClose = 32;
constexpr int kFooterHeight = 40;

/** How long the window waits for the page to sign out before exiting anyway. */
constexpr int kSignoutMs = 3000;

/** Run in an isolated world, so the page's own scripts can neither see nor
 *  replace it. The fetch is still the page's: same origin, its cookie, and an
 *  `Origin` header, which website-main requires of a write
 *  (docs/AUTH-BROKER.md, "The sign-in window"). A request from Qt's own network
 *  stack would carry none. */
const char kSignout[] = R"JS(
window.__mauvelySignout = 'pending';
fetch('/api/auth/signout', {method: 'POST', credentials: 'same-origin'})
  .then(() => { window.__mauvelySignout = 'done'; },
        () => { window.__mauvelySignout = 'failed'; });
)JS";

/** The mark, `com.mauvely.start` from os-image's design, with its
 *  `ColorScheme-Text` part in `ink`. The dot stays teal. */
QPixmap mark(const QColor &ink, int px, qreal dpr) {
    QFile file(QStringLiteral(":/signin/mark.svg"));
    if (!file.open(QIODevice::ReadOnly)) return {};
    QByteArray svg = file.readAll();
    svg.replace("color:#232629", "color:" + ink.name().toLatin1());
    QSvgRenderer renderer(svg);
    QPixmap pixmap(QSize(px, px) * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&p, QRectF(0, 0, px, px));
    return pixmap;
}

/** The header's ground and its brand band. Pressing anywhere on it that is not
 *  the close button moves the window. */
class Header : public QWidget {
public:
    explicit Header(FramelessWindow *window) : QWidget(window), m_window(window) {
        setFixedHeight(kHeaderHeight);
        setAttribute(Qt::WA_OpaquePaintEvent, true);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        const Theme &t = Theme::instance();
        QPainter p(this);
        // The brand's two colours across the chrome ground, faint, and at full
        // strength in the band on top: this is Mauvely's window, not a page's.
        p.fillRect(rect(), t.bgAlt());
        const qreal wash = t.isDark() ? 0.20 : 0.14;
        QLinearGradient tint(0, 0, width(), 0);
        tint.setColorAt(0, Theme::withAlpha(brand::violet().step(500), wash));
        tint.setColorAt(1, Theme::withAlpha(brand::mauve().step(500), wash * 0.7));
        p.fillRect(rect(), tint);
        QLinearGradient band(0, 0, width(), 0);
        band.setColorAt(0, brand::violet().step(500));
        band.setColorAt(1, brand::mauve().step(500));
        p.fillRect(QRect(0, 0, width(), kBand), band);
        p.fillRect(QRect(0, height() - 1, width(), 1), t.borderSubtle());
    }

    void mousePressEvent(QMouseEvent *e) override {
        if (e->button() == Qt::LeftButton) {
            m_window->beginSystemMove();
            e->accept();
            return;
        }
        QWidget::mousePressEvent(e);
    }

private:
    FramelessWindow *m_window;
};

/** A plain ×, drawn rather than taken from an icon font the window does not
 *  carry. */
class CloseButton : public QAbstractButton {
public:
    explicit CloseButton(QWidget *parent) : QAbstractButton(parent) {
        setFixedSize(kClose, kClose);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::TabFocus);
        setAccessibleName(QStringLiteral("Close"));
        setToolTip(QStringLiteral("Close"));
    }

protected:
    void paintEvent(QPaintEvent *) override {
        const Theme &t = Theme::instance();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        if (underMouse() || hasFocus()) {
            p.setPen(Qt::NoPen);
            p.setBrush(t.surfaceRaised());
            p.drawRoundedRect(QRectF(rect()), tokens::radius::kControl, tokens::radius::kControl);
        }
        QPen pen(underMouse() ? t.text() : t.textMuted(), 1.6);
        pen.setCapStyle(Qt::RoundCap);
        p.setPen(pen);
        const QRectF x = QRectF(rect()).adjusted(11, 11, -11, -11);
        p.drawLine(x.topLeft(), x.bottomRight());
        p.drawLine(x.topRight(), x.bottomLeft());
    }
    void enterEvent(QEnterEvent *e) override { update(); QAbstractButton::enterEvent(e); }
    void leaveEvent(QEvent *e) override { update(); QAbstractButton::leaveEvent(e); }
};

/** The strip under the page, on the chrome ground, for the one quiet way out. */
class Footer : public QWidget {
public:
    explicit Footer(QWidget *parent) : QWidget(parent) {
        setFixedHeight(kFooterHeight);
        setAttribute(Qt::WA_OpaquePaintEvent, true);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        const Theme &t = Theme::instance();
        QPainter p(this);
        p.fillRect(rect(), t.bgAlt());
        p.fillRect(QRect(0, 0, width(), 1), t.borderSubtle());
    }
};

/** The page's context menu, with editing and moving about and nothing else:
 *  no saving, no page source, no inspector. */
class View : public QWebEngineView {
public:
    using QWebEngineView::QWebEngineView;

protected:
    void contextMenuEvent(QContextMenuEvent *e) override {
        QMenu *menu = createStandardContextMenu();
        if (!menu) return;
        const QList<QAction *> allowed = {
            pageAction(QWebEnginePage::Back),       pageAction(QWebEnginePage::Forward),
            pageAction(QWebEnginePage::Reload),     pageAction(QWebEnginePage::Undo),
            pageAction(QWebEnginePage::Redo),       pageAction(QWebEnginePage::Cut),
            pageAction(QWebEnginePage::Copy),       pageAction(QWebEnginePage::Paste),
            pageAction(QWebEnginePage::PasteAndMatchStyle),
            pageAction(QWebEnginePage::SelectAll),  pageAction(QWebEnginePage::CopyLinkToClipboard),
        };
        for (QAction *a : menu->actions())
            if (!a->isSeparator() && !allowed.contains(a)) menu->removeAction(a);
        // No separator at either end or twice in a row.
        bool previousWasSeparator = true;
        for (QAction *a : menu->actions()) {
            if (a->isSeparator() && previousWasSeparator) menu->removeAction(a);
            else previousWasSeparator = a->isSeparator();
        }
        const QList<QAction *> left = menu->actions();
        if (!left.isEmpty() && left.last()->isSeparator()) menu->removeAction(left.last());
        if (menu->actions().isEmpty()) {
            delete menu;
            return;
        }
        menu->setAttribute(Qt::WA_DeleteOnClose, true);
        menu->popup(e->globalPos());
    }
};

}  // namespace

SigninWindow::SigninWindow(const QUrl &accounts, const QString &code, QWidget *parent)
    : FramelessWindow(parent), m_accounts(accounts) {
    setWindowTitle(QStringLiteral("Sign in to Mauvely"));
    setMinimumSize(400, 480);
    resize(kWidth, kHeight);

    auto *header = new Header(this);
    auto *row = new QHBoxLayout(header);
    row->setContentsMargins(tokens::space::k20, kBand, tokens::space::k12, 0);
    row->setSpacing(tokens::space::kInline);
    m_mark = new QLabel(header);
    m_mark->setFixedSize(kMark, kMark);
    m_mark->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    row->addWidget(m_mark);

    auto *titles = new QVBoxLayout;
    titles->setContentsMargins(0, 0, 0, 0);
    titles->setSpacing(0);
    m_title = new QLabel(QStringLiteral("Sign in to Mauvely"), header);
    m_host = new QLabel(header);
    for (QLabel *l : {m_title, m_host}) l->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    titles->addStretch(1);
    titles->addWidget(m_title);
    titles->addWidget(m_host);
    titles->addStretch(1);
    row->addLayout(titles, 1);

    auto *close = new CloseButton(header);
    connect(close, &QAbstractButton::clicked, this, &QWidget::close);
    row->addWidget(close, 0, Qt::AlignVCenter);
    setTitleBar(header);

    m_stack = new QStackedWidget(this);
    QWebEngineProfile *profile = SigninPage::makeProfile(this);
    m_view = new View(m_stack);
    m_page = new SigninPage(profile, accounts, code, m_view);
    m_view->setPage(m_page);
    m_stack->addWidget(m_view);

    // The code, for typing into the page in the browser.
    m_codeText = code;
    m_codePanel = new QWidget(m_stack);
    m_codePanel->setAutoFillBackground(true);
    auto *panel = new QVBoxLayout(m_codePanel);
    panel->setContentsMargins(tokens::space::k32, 0, tokens::space::k32, 0);
    panel->setSpacing(tokens::space::k16);
    m_codeHeading = new QLabel(QStringLiteral("Enter this code in your browser."), m_codePanel);
    m_code = new QLabel(code, m_codePanel);
    m_code->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_codeNote = new QLabel(
        QStringLiteral("The sign-in page is open there. This window closes once you approve."),
        m_codePanel);
    for (QLabel *l : {m_codeHeading, m_code, m_codeNote}) {
        l->setAlignment(Qt::AlignHCenter);
        l->setWordWrap(l != m_code);
    }
    panel->addStretch(1);
    panel->addWidget(m_codeHeading);
    panel->addWidget(m_code);
    panel->addWidget(m_codeNote);
    panel->addStretch(2);
    m_stack->addWidget(m_codePanel);
    setBody(m_stack);

    // Always there, and quiet: some sign-ins refuse an embedded browser.
    auto *footer = new Footer(this);
    auto *strip = new QHBoxLayout(footer);
    strip->setContentsMargins(0, 1, 0, 0);
    m_switch = new QPushButton(footer);
    m_switch->setFlat(true);
    m_switch->setCursor(Qt::PointingHandCursor);
    connect(m_switch, &QPushButton::clicked, this, [this] { useBrowser(!m_inBrowser); });
    strip->addStretch(1);
    strip->addWidget(m_switch);
    strip->addStretch(1);
    setStatusBar(footer);
    useBrowser(false);

    connect(m_page, &QWebEnginePage::urlChanged, this,
            [this](const QUrl &url) { m_host->setText(navpolicy::hostLine(url)); });
    // A renderer that died leaves nothing to sign in with: exit as a failure,
    // so mauvely-session shows the code another way.
    connect(m_page, &QWebEnginePage::renderProcessTerminated, this,
            [](QWebEnginePage::RenderProcessTerminationStatus status, int) {
                if (status != QWebEnginePage::NormalTerminationStatus)
                    QCoreApplication::exit(3);
            });
    // The page has no reload button of its own when the network failed.
    auto *reload = new QShortcut(QKeySequence::Refresh, this);
    connect(reload, &QShortcut::activated, m_view, &QWebEngineView::reload);

    // The window never closes by itself: it hides, and `finish()` exits.
    setCloseGuard([this] {
        personClosed();
        return false;
    });

    connect(&Theme::instance(), &Theme::changed, this, &SigninWindow::restyle);
    restyle();
}

bool SigninWindow::event(QEvent *e) {
    // The mark is drawn for the screen's pixel ratio, which is known only once
    // the window is on a screen, and changes when it moves to another.
    if (e->type() == QEvent::Show || e->type() == QEvent::DevicePixelRatioChange)
        m_mark->setPixmap(mark(Theme::instance().text(), kMark, devicePixelRatioF()));
    return FramelessWindow::event(e);
}

void SigninWindow::load(const QUrl &page) {
    m_host->setText(navpolicy::hostLine(page));
    m_view->load(page);
    // Typing goes to the page, not to the close button first in the chain.
    m_view->setFocus();
}

void SigninWindow::restyle() {
    const Theme &t = Theme::instance();
    m_mark->setPixmap(mark(t.text(), kMark, devicePixelRatioF()));
    m_title->setFont(style::display(tokens::type::kLg));
    m_host->setFont(style::mono(tokens::type::k2Xs));
    QPalette title = m_title->palette();
    title.setColor(QPalette::WindowText, t.text());
    m_title->setPalette(title);
    QPalette host = m_host->palette();
    host.setColor(QPalette::WindowText, t.textFaint());
    m_host->setPalette(host);
    m_view->page()->setBackgroundColor(t.bg());

    QPalette panel = m_codePanel->palette();
    panel.setColor(QPalette::Window, t.bg());
    m_codePanel->setPalette(panel);
    m_codeHeading->setFont(style::display(tokens::type::kXl));
    QFont code = style::mono(30);
    code.setLetterSpacing(QFont::PercentageSpacing, 110);
    m_code->setFont(code);
    m_codeNote->setFont(style::body(tokens::type::kMd));
    const auto ink = [](QLabel *l, const QColor &c) {
        QPalette pal = l->palette();
        pal.setColor(QPalette::WindowText, c);
        l->setPalette(pal);
    };
    ink(m_codeHeading, t.text());
    ink(m_code, t.text());
    ink(m_codeNote, t.textMuted());

    m_switch->setFont(style::body(tokens::type::kSm, tokens::type::kMedium));
    m_switch->setStyleSheet(QStringLiteral(
        "QPushButton { border: 0; background: transparent; padding: 0 8px; color: %1; }"
        "QPushButton:hover, QPushButton:focus { color: %2; text-decoration: underline; }")
                                .arg(t.textMuted().name(), t.primaryText().name()));
    update();
}

void SigninWindow::useBrowser(bool browser) {
    m_inBrowser = browser;
    m_stack->setCurrentWidget(browser ? m_codePanel : static_cast<QWidget *>(m_view));
    m_switch->setText(browser ? QStringLiteral("Sign in here instead")
                              : QStringLiteral("Use your browser instead"));
    if (!browser) return;
    // The page without the code: the browser's address reaches an argv.
    QUrl link = m_accounts;
    link.setPath(QStringLiteral("/link"));
    if (navpolicy::openable(link, m_codeText)) openOutside(link);
}

void SigninWindow::personClosed() {
    if (m_finishing || m_closed) return;
    m_closed = true;
    hide();
    // With the code on screen, the person is carrying on in the browser: the
    // link goes on, and mauvely-session finishes the window when it ends.
    if (!m_inBrowser) emit closedByPerson();
}

void SigninWindow::finish() {
    if (m_finishing) return;
    m_finishing = true;
    hide();
    signOutThen([] { QCoreApplication::exit(0); });
}

void SigninWindow::signOutThen(std::function<void()> then) {
    // Only from the accounts origin: anywhere else `/api/auth/signout` is
    // somebody else's address, and the person has no Mauvely session there.
    if (!navpolicy::sameOrigin(m_page->url(), m_accounts)) {
        then();
        return;
    }
    m_page->runJavaScript(QString::fromLatin1(kSignout), QWebEngineScript::ApplicationWorld);
    const QDeadlineTimer deadline(kSignoutMs);
    auto *poll = new QTimer(this);
    poll->setInterval(50);
    auto done = std::make_shared<bool>(false);
    const auto end = [poll, done, then] {
        if (*done) return;
        *done = true;
        poll->stop();
        then();
    };
    connect(poll, &QTimer::timeout, this, [this, deadline, end] {
        if (deadline.hasExpired()) {
            end();
            return;
        }
        m_page->runJavaScript(QStringLiteral("window.__mauvelySignout"),
                              QWebEngineScript::ApplicationWorld, [end](const QVariant &state) {
                                  const QString s = state.toString();
                                  if (s == QLatin1String("done") || s == QLatin1String("failed"))
                                      end();
                              });
    });
    poll->start();
}
