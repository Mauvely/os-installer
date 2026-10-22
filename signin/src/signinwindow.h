#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  SigninWindow — "Sign in to Mauvely": a frameless window around the
//  device-link page.
//
//  The header is the window's own decoration: the Mauvely mark, the title, the
//  host of the page on screen and a close button, over a violet-to-mauve band
//  that marks it as Mauvely's and not a web page's. Dragging the header moves
//  the window. Below it, the page.
//
//  The host line is there because the page changes hands: single sign-on
//  takes the window to the organisation's identity provider, and a window
//  with no address bar should still say whose page the person is typing into.
//
//  A quiet "Use your browser instead" is always under the page, because some
//  sign-ins refuse to run in an embedded browser (Google's does). It opens the
//  link page in the default browser, without the code, and the window shows
//  the code to type there.
//
//  Closing the window while it shows the sign-in is "Later": it hides, says so
//  (`closedByPerson`), and waits. mauvely-session looks at the link once more,
//  in case the person approved just before closing, and then finishes the
//  window. Closing it while it shows the code only hides it: the person is
//  carrying on in their browser.
//
//  `finish()` is the one way out. The window goes, the session the person
//  made in it is signed out from the page, and the process exits 0. It comes
//  after mauvely-session's poll has claimed the link: the claim re-checks the
//  approving session, so signing out first would burn the link.
// ─────────────────────────────────────────────────────────────────────────────

#include "ui/framelesswindow.h"

#include <QUrl>

class QLabel;
class QPushButton;
class QStackedWidget;
class QWebEngineView;
class SigninPage;

class SigninWindow : public FramelessWindow {
    Q_OBJECT
public:
    SigninWindow(const QUrl &accounts, const QString &code, QWidget *parent = nullptr);

    /** Loads `page`, the device-link page with its code. */
    void load(const QUrl &page);

    /** mauvely-session is done with the link: sign out, then exit 0. Once
     *  only. */
    void finish();

signals:
    /** The person closed the window while it showed the sign-in. */
    void closedByPerson();

protected:
    bool event(QEvent *e) override;

private:
    void personClosed();
    void useBrowser(bool browser);
    void restyle();
    void signOutThen(std::function<void()> then);

    QUrl m_accounts;
    QLabel *m_mark = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_host = nullptr;
    QStackedWidget *m_stack = nullptr;
    QWebEngineView *m_view = nullptr;
    SigninPage *m_page = nullptr;
    QWidget *m_codePanel = nullptr;
    QLabel *m_codeHeading = nullptr;
    QLabel *m_code = nullptr;
    QLabel *m_codeNote = nullptr;
    QPushButton *m_switch = nullptr;
    QString m_codeText;
    bool m_inBrowser = false;
    bool m_closed = false;
    bool m_finishing = false;
};
