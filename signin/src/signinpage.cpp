// ─────────────────────────────────────────────────────────────────────────────
//  SigninPage — see signinpage.h.
// ─────────────────────────────────────────────────────────────────────────────

#include "signinpage.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QProcess>
#include <QTimer>
#include <QWebEngineDownloadRequest>
#include <QWebEngineFileSystemAccessRequest>
#include <QWebEngineFullScreenRequest>
#include <QWebEngineNewWindowRequest>
#include <QWebEnginePermission>
#include <QWebEngineProfile>
#include <QWebEngineRegisterProtocolHandlerRequest>
#include <QWebEngineSettings>
#include <QWebEngineWebAuthUxRequest>

namespace {

navpolicy::Kind kindOf(QWebEnginePage::NavigationType type) {
    switch (type) {
    case QWebEnginePage::NavigationTypeLinkClicked: return navpolicy::Kind::Link;
    case QWebEnginePage::NavigationTypeTyped: return navpolicy::Kind::Typed;
    case QWebEnginePage::NavigationTypeFormSubmitted: return navpolicy::Kind::Form;
    case QWebEnginePage::NavigationTypeBackForward: return navpolicy::Kind::BackForward;
    case QWebEnginePage::NavigationTypeReload: return navpolicy::Kind::Reload;
    case QWebEnginePage::NavigationTypeRedirect: return navpolicy::Kind::Redirect;
    case QWebEnginePage::NavigationTypeOther: return navpolicy::Kind::Other;
    }
    return navpolicy::Kind::Other;
}

}  // namespace

void openOutside(const QUrl &url) {
    const QString address = url.toString(QUrl::FullyEncoded);
    QTimer::singleShot(0, qApp, [address] {
        // No portal: xdg-open, with the address as an argument of its own. It
        // is an http(s) or mailto: address (navpolicy::openable), so never an
        // option.
        const auto xdgOpen = [address] {
            QProcess::startDetached(QStringLiteral("xdg-open"), {address});
        };
        if (!QDBusConnection::sessionBus().isConnected()) {
            xdgOpen();
            return;
        }
        QDBusMessage call = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.portal.Desktop"),
            QStringLiteral("/org/freedesktop/portal/desktop"),
            QStringLiteral("org.freedesktop.portal.OpenURI"), QStringLiteral("OpenURI"));
        call << QString() << address << QVariantMap();
        auto *watcher = new QDBusPendingCallWatcher(
            QDBusConnection::sessionBus().asyncCall(call, 5000), qApp);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, qApp,
                         [xdgOpen](QDBusPendingCallWatcher *w) {
                             w->deleteLater();
                             if (w->isError()) xdgOpen();
                         });
    });
}

SigninPage::SigninPage(QWebEngineProfile *profile, const QUrl &accounts, const QString &code,
                       QObject *parent)
    : QWebEnginePage(profile, parent), m_accounts(accounts), m_code(code) {
    // createWindow() is left as QWebEnginePage's, which makes no page, so every
    // new-window request arrives here with its address.
    connect(this, &QWebEnginePage::newWindowRequested, this,
            [this](QWebEngineNewWindowRequest &request) {
                if (navpolicy::newWindow(request.requestedUrl(), context())
                    == navpolicy::Action::External)
                    openOutside(request.requestedUrl());
            });
    connect(this, &QWebEnginePage::permissionRequested, this,
            [](QWebEnginePermission permission) { permission.deny(); });
    connect(this, &QWebEnginePage::fullScreenRequested, this,
            [](QWebEngineFullScreenRequest request) { request.reject(); });
    connect(this, &QWebEnginePage::fileSystemAccessRequested, this,
            [](QWebEngineFileSystemAccessRequest request) { request.reject(); });
    connect(this, &QWebEnginePage::registerProtocolHandlerRequested, this,
            [](QWebEngineRegisterProtocolHandlerRequest request) { request.reject(); });
    // A passkey ceremony needs a prompt this window does not draw (the
    // account, the PIN, "touch your key"). Cancelled at once, the page says so
    // and the person signs in another way, instead of waiting on nothing.
    connect(this, &QWebEnginePage::webAuthUxRequested, this,
            [](QWebEngineWebAuthUxRequest *request) { request->cancel(); });
}

QWebEngineProfile *SigninPage::makeProfile(QObject *parent) {
    // No storage name: off the record. Cookies, cache and storage live in
    // memory and go with the process.
    auto *profile = new QWebEngineProfile(parent);
    profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
    profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
    profile->setSpellCheckEnabled(false);
    QObject::connect(profile, &QWebEngineProfile::downloadRequested, profile,
                     [](QWebEngineDownloadRequest *download) { download->cancel(); });

    QWebEngineSettings *s = profile->settings();
    // A popup the page opens without a click is not followed at all; one
    // opened by a click goes to the browser.
    s->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
    s->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, false);
    s->setAttribute(QWebEngineSettings::JavascriptCanPaste, false);
    // A link or a file dropped on the window would otherwise load in it.
    s->setAttribute(QWebEngineSettings::NavigateOnDropEnabled, false);
    s->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
    s->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, false);
    s->setAttribute(QWebEngineSettings::PluginsEnabled, false);
    s->setAttribute(QWebEngineSettings::PdfViewerEnabled, false);
    s->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, false);
    s->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, false);
    s->setAttribute(QWebEngineSettings::AllowWindowActivationFromJavaScript, false);
    s->setAttribute(QWebEngineSettings::WebRTCPublicInterfacesOnly, true);
    s->setAttribute(QWebEngineSettings::DnsPrefetchEnabled, false);
    s->setAttribute(QWebEngineSettings::HyperlinkAuditingEnabled, false);
    return profile;
}

navpolicy::Context SigninPage::context() const {
    return {m_accounts, url(), m_code};
}

bool SigninPage::acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) {
    switch (navpolicy::decide(url, kindOf(type), isMainFrame, context())) {
    case navpolicy::Action::Load:
        return true;
    case navpolicy::Action::External:
        openOutside(url);
        return false;
    case navpolicy::Action::Refuse:
        return false;
    }
    return false;
}

QStringList SigninPage::chooseFiles(FileSelectionMode, const QStringList &, const QStringList &) {
    return {};
}

void SigninPage::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel, const QString &, int,
                                          const QString &) {}
