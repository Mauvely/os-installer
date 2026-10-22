// ─────────────────────────────────────────────────────────────────────────────
//  navpolicy — see navpolicy.h.
// ─────────────────────────────────────────────────────────────────────────────

#include "navpolicy.h"

#include "protocol.h"

namespace navpolicy {

namespace {

int defaultPort(const QString &scheme) {
    if (scheme == QLatin1String("https")) return 443;
    if (scheme == QLatin1String("http")) return 80;
    return -1;
}

bool isWeb(const QUrl &url) {
    const QString scheme = url.scheme();
    return scheme == QLatin1String("https") || scheme == QLatin1String("http");
}

}  // namespace

bool sameOrigin(const QUrl &a, const QUrl &b) {
    if (!a.isValid() || !b.isValid()) return false;
    const QString scheme = a.scheme().toLower();
    if (scheme.isEmpty() || scheme != b.scheme().toLower()) return false;
    if (a.host().isEmpty() || a.host().compare(b.host(), Qt::CaseInsensitive) != 0)
        return false;
    return a.port(defaultPort(scheme)) == b.port(defaultPort(scheme));
}

bool secureEnough(const QUrl &url) {
    if (url.scheme() == QLatin1String("https")) return !url.host().isEmpty();
    return url.scheme() == QLatin1String("http") && protocol::isLocalHost(url.host());
}

bool openable(const QUrl &url, const QString &code) {
    if (!url.isValid()) return false;
    if (!isWeb(url) && url.scheme() != QLatin1String("mailto")) return false;
    if (!code.isEmpty()) {
        // Decoded and encoded both: a page could spell the code either way.
        const QByteArray encoded = url.toEncoded();
        if (QUrl::fromPercentEncoding(encoded).contains(code, Qt::CaseInsensitive)) return false;
        if (QString::fromLatin1(encoded).contains(code, Qt::CaseInsensitive)) return false;
    }
    return true;
}

Action decide(const QUrl &url, Kind kind, bool isMainFrame, const Context &context) {
    const QString scheme = url.scheme();

    if (!isMainFrame) {
        // A frame cannot take the window anywhere; it may load the web.
        const bool web = isWeb(url) || scheme == QLatin1String("about")
                         || scheme == QLatin1String("data") || scheme == QLatin1String("blob");
        return web ? Action::Load : Action::Refuse;
    }

    if (scheme == QLatin1String("about") && url.path() == QLatin1String("blank"))
        return Action::Load;

    // Anything that is not the web, or is the web in clear to a real host,
    // never loads here. A click on one is the person's to follow elsewhere.
    if (!isWeb(url) || !secureEnough(url)) {
        if (kind == Kind::Link && openable(url, context.code)) return Action::External;
        return Action::Refuse;
    }

    if (sameOrigin(url, context.accounts)) return Action::Load;

    switch (kind) {
    case Kind::Redirect:
    case Kind::Form:
    case Kind::Other:
    case Kind::BackForward:
    case Kind::Reload:
        return Action::Load;
    case Kind::Link:
        if (sameOrigin(url, context.current)) return Action::Load;
        return openable(url, context.code) ? Action::External : Action::Refuse;
    case Kind::Typed:
        // There is no address bar: the only typed load is the window's own,
        // on the accounts origin.
        return Action::Refuse;
    }
    return Action::Refuse;
}

QString hostLine(const QUrl &url) {
    if (url.host().isEmpty()) return {};
    const QString host = url.host().contains(QLatin1Char(':'))
                             ? QLatin1Char('[') + url.host() + QLatin1Char(']')
                             : url.host();
    const int fallback = defaultPort(url.scheme());
    const int port = url.port(fallback);
    return port == fallback ? host : host + QLatin1Char(':') + QString::number(port);
}

Action newWindow(const QUrl &url, const Context &context) {
    return openable(url, context.code) ? Action::External : Action::Refuse;
}

}  // namespace navpolicy
