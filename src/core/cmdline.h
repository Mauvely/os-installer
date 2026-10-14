#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  cmdline — reading the kernel command line the way the kernel does.
//
//  Two things are read from it: `mauvely.server=<url>`, which overrides the
//  default server for local testing, and `rd.live.image`, without which the
//  installer refuses to start at all. A developer running the binary on their
//  own machine must never get as far as a list of their disks.
// ─────────────────────────────────────────────────────────────────────────────

#include <QString>
#include <QStringList>

namespace cmdline {

/** The arguments, split on whitespace outside double quotes, quotes dropped:
 *  `"a=b c"` and `a="b c"` are both the one argument `a=b c`. */
QStringList split(const QString &cmdline);

/** The value of the last `key=value`, or a null string when the key is absent. */
QString value(const QString &cmdline, const QString &key);

/** Whether `flag` appears as a bare argument. */
bool hasFlag(const QString &cmdline, const QString &flag);

}  // namespace cmdline
