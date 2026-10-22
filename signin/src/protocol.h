#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  protocol — what mauvely-session tells the window, on standard input.
//
//  Line 1, sent once at start:
//
//      {"code":"ABCD-EFGH","origin":"https://mauvely.com"}
//
//  Then, when its poll of the device link succeeds, is declined or expires,
//  mauvely-session writes a second line (`finish`) and closes the pipe. Any
//  input after the first line, or the end of input, finishes the window: it
//  signs its temporary session out and exits 0.
//
//  The window says one thing back, on standard output: `closed`, when the
//  person closes it while it shows the sign-in. It is hidden by then and
//  waits for `finish`, so that mauvely-session can look at the link once more
//  before the page's session is signed out.
//
//  The code arrives here and nowhere else. Every local user can read a
//  process's argv and environment under /proc, and with the code any of them
//  could approve this computer's link for their own account. So nothing is
//  read from argv, and this file is the only way in.
// ─────────────────────────────────────────────────────────────────────────────

#include <QByteArray>
#include <QString>
#include <QUrl>

#include <optional>

namespace protocol {

/** The longest first line read. The real one is under 100 bytes. */
constexpr int kMaxLine = 4096;

struct Start {
    QString code;
    QUrl origin;
};

/** Letters, digits and `-`, at most 32: the server's code is `ABCD-EFGH`. The
 *  same rule as mauvely-session's, which never sends anything else. */
bool plainCode(const QString &code);

/** The hosts plain HTTP is allowed to: the local stack, and the host as a qemu
 *  VM sees it. The same list as mauvely-session's `origin_for`. */
bool isLocalHost(const QString &host);

/** An accounts origin: https, or http to a local host, with a host and nothing
 *  after it — no user, path, query or fragment. */
bool acceptableOrigin(const QUrl &origin);

/** Parses the first line. On failure, `why` says what was wrong, without
 *  repeating the input: it may hold a code. */
std::optional<Start> parseStart(const QByteArray &line, QString *why = nullptr);

/** `<origin>/link?code=<code>&embed=os`: website-main's device-link page,
 *  with the code filled in and the compact layout asked for. */
QUrl pageUrl(const Start &start);

/** Reads one line from `fd`, without the newline, a byte at a time so that
 *  nothing after it is consumed. False at end of input before a newline, on a
 *  read error, or past kMaxLine. */
bool readLine(int fd, QByteArray *line);

}  // namespace protocol
