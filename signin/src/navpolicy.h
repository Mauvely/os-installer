#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  navpolicy — where the sign-in window may go, decided in one pure function.
//
//  The window shows website-main's device-link page and whatever the sign-in
//  on it needs, and nothing else. In the main frame:
//
//    the accounts origin                      load, whatever started it
//    a redirect or a form post, anywhere      load: single sign-on goes to the
//                                             organisation's identity
//                                             provider and comes back by both
//    a navigation the page made itself        load, for the same reason: an
//      (script, meta refresh, back, reload)   identity provider moves between
//                                             its own pages that way
//    a link the person clicked, to the        load: an identity provider's own
//      page's own origin                      "Use another account"
//    a link the person clicked, anywhere      the default browser, and the
//      else                                   window stays where it is
//    a new window, `target=_blank` included   the default browser
//    plain http to anything but a local host  never loaded; a click on one goes
//                                             to the browser
//    any other scheme                         refused; a clicked mailto: goes
//                                             out
//
//  Frames inside the page may load the web and nothing else.
//
//  Nothing whose address holds the code is ever handed to the browser:
//  opening it puts the address in an argv every local user can read.
// ─────────────────────────────────────────────────────────────────────────────

#include <QString>
#include <QUrl>

namespace navpolicy {

enum class Action { Load, External, Refuse };

/** QWebEnginePage::NavigationType, without the dependency. */
enum class Kind { Link, Typed, Form, BackForward, Reload, Redirect, Other };

struct Context {
    QUrl accounts;  // the accounts origin
    QUrl current;   // the page the main frame is on now
    QString code;   // the device-link code
};

/** Scheme, host and port, with the scheme's default port filled in. */
bool sameOrigin(const QUrl &a, const QUrl &b);

/** https, or http to a local host. */
bool secureEnough(const QUrl &url);

/** What may be given to the default browser: http(s) or mailto:, without the
 *  code anywhere in it. */
bool openable(const QUrl &url, const QString &code);

Action decide(const QUrl &url, Kind kind, bool isMainFrame, const Context &context);

/** What the header says about where the page is: the host, and the port when
 *  it is not the scheme's own. */
QString hostLine(const QUrl &url);

/** A new window: the browser, or nothing. */
Action newWindow(const QUrl &url, const Context &context);

}  // namespace navpolicy
