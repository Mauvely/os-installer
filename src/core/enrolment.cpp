#include "core/enrolment.h"

#include "core/cmdline.h"

#include <QByteArray>
#include <QUrl>

namespace enrolment {

QString initialServer(const QString &kernelCmdline) {
    const QString arg = cmdline::value(kernelCmdline, serverArgument());
    return !arg.isNull() && isServerUrl(arg) ? normaliseServer(arg) : defaultServer();
}

bool isServerUrl(const QString &url) {
    const QString t = url.trimmed();
    if (t.isEmpty() || t.contains(QLatin1Char(' '))) return false;
    const QUrl u(t, QUrl::StrictMode);
    const QString scheme = u.scheme();
    return u.isValid() && (scheme == QLatin1String("https") || scheme == QLatin1String("http")) &&
           !u.host().isEmpty() && !u.hasQuery() && !u.hasFragment() && u.userInfo().isEmpty();
}

QString normaliseServer(const QString &url) {
    QString t = url.trimmed();
    while (t.endsWith(QLatin1Char('/'))) t.chop(1);
    return t;
}

QString normaliseToken(const QString &token) { return token.trimmed(); }

bool isTokenWellFormed(const QString &token) {
    if (token.size() != kTokenLength || !token.startsWith(QLatin1String("mvd_"))) return false;
    const QByteArray body = token.mid(4).toLatin1();
    for (const char c : body) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                        (c >= '0' && c <= '9') || c == '-' || c == '_';
        if (!ok) return false;
    }
    const auto opts = QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals;
    const auto decoded =
        QByteArray::fromBase64Encoding(body, opts | QByteArray::AbortOnBase64DecodingErrors);
    return decoded && decoded.decoded.size() == 32 && decoded.decoded.toBase64(opts) == body;
}

}  // namespace enrolment
