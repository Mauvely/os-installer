#include "core/cmdline.h"

namespace cmdline {

QStringList split(const QString &cmdline) {
    QStringList args;
    QString cur;
    bool quoted = false, any = false;
    for (const QChar c : cmdline) {
        if (c == QLatin1Char('"')) {
            quoted = !quoted;
            any = true;
        } else if (c.isSpace() && !quoted) {
            if (any) args << cur;
            cur.clear();
            any = false;
        } else {
            cur += c;
            any = true;
        }
    }
    if (any) args << cur;
    return args;
}

QString value(const QString &cmdline, const QString &key) {
    QString found;  // null until seen
    const QString prefix = key + QLatin1Char('=');
    for (const QString &a : split(cmdline))
        if (a.startsWith(prefix)) found = a.mid(prefix.size());  // the last one wins
    return found;
}

bool hasFlag(const QString &cmdline, const QString &flag) {
    return split(cmdline).contains(flag);
}

}  // namespace cmdline
