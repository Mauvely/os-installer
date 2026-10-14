#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  enrolment — the server address and the token, checked offline.
//
//  The installer never talks to the server. There is no endpoint that checks a
//  token without spending it, and the device key the token is spent with has to
//  be made and sealed by the installed system, not by this live one. So a token
//  is checked for its *format* only: `mvd_` plus 32 random bytes in unpadded
//  base64url (brief 19, "Tokens"). An expired or used-up token surfaces on the
//  installed machine's first check-in.
// ─────────────────────────────────────────────────────────────────────────────

#include <QString>

namespace enrolment {

/** Production. The local stack, seen from a qemu VM, is http://10.0.2.2:8798. */
inline QString defaultServer() { return QStringLiteral("https://devices.mauvely.com"); }

/** The kernel argument that overrides `defaultServer()`. */
inline QString serverArgument() { return QStringLiteral("mauvely.server"); }

/** What the server field starts with: the kernel argument when it is a usable
 *  address, the default otherwise. */
QString initialServer(const QString &kernelCmdline);

/** An http or https URL with a host, and no query or fragment. http is allowed
 *  because the local stack is plain http. */
bool isServerUrl(const QString &url);

/** Trimmed, without a trailing slash: what is written to /etc/mauvely/server. */
QString normaliseServer(const QString &url);

/** Surrounding whitespace removed, which is all a paste usually adds. */
QString normaliseToken(const QString &token);

/** `mvd_` and 43 base64url characters that decode to exactly 32 bytes and
 *  re-encode to themselves, so a mistyped last character is caught too. */
bool isTokenWellFormed(const QString &token);

/** 4 + 43. Shown in the error, so the user can count. */
constexpr int kTokenLength = 47;

}  // namespace enrolment
