#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  SigninPage — the web page, contained.
//
//  - An off-the-record profile: nothing reaches the disk, and the session the
//    person makes here ends with the process (and is signed out before that,
//    see SigninWindow).
//  - No devtools, no downloads, no file chooser, no full screen, and every
//    permission the page asks for is denied without a prompt.
//  - Navigation follows navpolicy; a new window, or a link off the sign-in,
//    goes to the default browser (`openOutside`).
//  - The page's console is not printed: its messages carry the page's address,
//    and the address carries the code.
// ─────────────────────────────────────────────────────────────────────────────

#include "navpolicy.h"

#include <QWebEnginePage>

class QWebEngineProfile;

class SigninPage : public QWebEnginePage {
    Q_OBJECT
public:
    SigninPage(QWebEngineProfile *profile, const QUrl &accounts, const QString &code,
               QObject *parent = nullptr);

    /** An off-the-record profile with downloads cancelled and the settings
     *  this window wants. */
    static QWebEngineProfile *makeProfile(QObject *parent);

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override;
    QStringList chooseFiles(FileSelectionMode mode, const QStringList &oldFiles,
                            const QStringList &acceptedMimeTypes) override;
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString &message,
                                  int lineNumber, const QString &sourceID) override;

private:
    navpolicy::Context context() const;

    QUrl m_accounts;
    QString m_code;
};

/** Hands `url` to the default browser: the OpenURI portal, or `xdg-open` with
 *  the address as its one argument when there is no portal. Queued, so it never
 *  runs inside a navigation. */
void openOutside(const QUrl &url);
