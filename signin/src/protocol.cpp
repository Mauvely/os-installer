// ─────────────────────────────────────────────────────────────────────────────
//  protocol — see protocol.h.
// ─────────────────────────────────────────────────────────────────────────────

#include "protocol.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

#include <cerrno>
#include <unistd.h>

namespace protocol {

bool plainCode(const QString &code) {
    if (code.isEmpty() || code.size() > 32) return false;
    for (const QChar c : code) {
        const char16_t u = c.unicode();
        const bool ok = (u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z')
                        || (u >= '0' && u <= '9') || u == '-';
        if (!ok) return false;
    }
    return true;
}

bool isLocalHost(const QString &host) {
    // QUrl gives an IPv6 literal without its brackets.
    return host == QLatin1String("localhost") || host == QLatin1String("127.0.0.1")
           || host == QLatin1String("::1") || host == QLatin1String("10.0.2.2");
}

bool acceptableOrigin(const QUrl &origin) {
    if (!origin.isValid() || origin.host().isEmpty()) return false;
    if (!origin.userInfo().isEmpty() || origin.hasQuery() || origin.hasFragment()) return false;
    if (!origin.path().isEmpty()) return false;
    const QString scheme = origin.scheme();
    if (scheme == QLatin1String("https")) return true;
    return scheme == QLatin1String("http") && isLocalHost(origin.host());
}

std::optional<Start> parseStart(const QByteArray &line, QString *why) {
    const auto fail = [why](const char *reason) -> std::optional<Start> {
        if (why) *why = QString::fromLatin1(reason);
        return std::nullopt;
    };
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(line, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return fail("the first line is not a JSON object");
    const QJsonObject o = doc.object();
    if (!o.value(QLatin1String("code")).isString()) return fail("no code");
    if (!o.value(QLatin1String("origin")).isString()) return fail("no origin");
    Start start;
    start.code = o.value(QLatin1String("code")).toString();
    if (!plainCode(start.code)) return fail("the code is not plainly a code");
    start.origin = QUrl(o.value(QLatin1String("origin")).toString(), QUrl::StrictMode);
    if (!acceptableOrigin(start.origin)) return fail("the origin is not an accounts origin");
    return start;
}

QUrl pageUrl(const Start &start) {
    QUrl url = start.origin;
    url.setPath(QStringLiteral("/link"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("code"), start.code);
    query.addQueryItem(QStringLiteral("embed"), QStringLiteral("os"));
    url.setQuery(query);
    return url;
}

bool readLine(int fd, QByteArray *line) {
    line->clear();
    for (;;) {
        char c = 0;
        const ssize_t n = ::read(fd, &c, 1);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        if (c == '\n') return true;
        if (line->size() >= kMaxLine) return false;
        line->append(c);
    }
}

}  // namespace protocol
