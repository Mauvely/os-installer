// ─────────────────────────────────────────────────────────────────────────────
//  mauvely-signin — Mauvely OS's sign-in window.
//
//  mauvely-session starts it when this computer is not linked to the person's
//  Mauvely account, writes the device-link code and the accounts origin on its
//  standard input (protocol.h), and tells it to finish when the link has been
//  approved, declined or has expired. The person signs in and approves in
//  website-main's own page, inside the window.
//
//  Exit status: 0 when finished, 2 when the first line is not what protocol.h
//  describes, 3 when the page's renderer died. Anything else, a signal
//  included, is Qt failing to start.
// ─────────────────────────────────────────────────────────────────────────────

#include "core/theme.h"
#include "protocol.h"
#include "signinwindow.h"
#include "ui/style.h"

#include <QApplication>
#include <QPalette>
#include <QSocketNotifier>
#include <QStyleHints>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

namespace {

Theme::Mode modeFor(Qt::ColorScheme scheme, Theme::Mode otherwise) {
    switch (scheme) {
    case Qt::ColorScheme::Dark: return Theme::Mode::Dark;
    case Qt::ColorScheme::Light: return Theme::Mode::Light;
    case Qt::ColorScheme::Unknown: break;
    }
    return otherwise;
}

}  // namespace

int main(int argc, char *argv[]) {
    // Either would let another local user reach the page: a remote-debugging
    // port listens on localhost for every uid, and the page holds the code.
    qunsetenv("QTWEBENGINE_REMOTE_DEBUGGING");
    qunsetenv("QTWEBENGINE_CHROMIUM_FLAGS");

    // A "closed" written after mauvely-session has gone must not kill the
    // window before it signs out.
    std::signal(SIGPIPE, SIG_IGN);

    // The code and the origin, before anything else starts.
    QByteArray line;
    if (!protocol::readLine(STDIN_FILENO, &line)) {
        std::fprintf(stderr, "mauvely-signin: no first line on standard input\n");
        return 2;
    }
    QString why;
    const std::optional<protocol::Start> start = protocol::parseStart(line, &why);
    line.fill('\0');
    if (!start) {
        std::fprintf(stderr, "mauvely-signin: %s\n", qPrintable(why));
        return 2;
    }

    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    // argv[0] alone: Qt and Chromium both take switches from the command line,
    // and nothing this window needs comes that way.
    int qtArgc = 1;
    char *qtArgv[] = {argc > 0 ? argv[0] : const_cast<char *>("mauvely-signin"), nullptr};
    QApplication app(qtArgc, qtArgv);
    QApplication::setApplicationName(QStringLiteral("mauvely-signin"));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    // The Wayland app id, and the desktop file that names the window.
    QGuiApplication::setDesktopFileName(QStringLiteral("com.mauvely.signin"));
    // The window hides before it signs out, and exits when that is done.
    app.setQuitOnLastWindowClosed(false);

    style::loadFonts();
    // Light or dark from the desktop's colour scheme, and following it. With
    // no answer, the palette the platform gave says which.
    QStyleHints *hints = QGuiApplication::styleHints();
    const Theme::Mode fromPalette = QGuiApplication::palette().color(QPalette::Window).lightness() < 128
                                        ? Theme::Mode::Dark
                                        : Theme::Mode::Light;
    Theme::instance().setMode(modeFor(hints->colorScheme(), fromPalette));
    Theme::instance().apply();
    QObject::connect(hints, &QStyleHints::colorSchemeChanged, &app, [](Qt::ColorScheme scheme) {
        Theme::instance().setMode(modeFor(scheme, Theme::instance().mode()));
    });

    SigninWindow window(start->origin, start->code);
    QObject::connect(&window, &SigninWindow::closedByPerson, &app, [] {
        std::fputs("closed\n", stdout);
        std::fflush(stdout);
    });
    window.load(protocol::pageUrl(*start));
    window.show();
    window.raise();
    window.activateWindow();

    // Anything more on standard input, or its end, finishes the window.
    ::fcntl(STDIN_FILENO, F_SETFL, ::fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
    QSocketNotifier input(STDIN_FILENO, QSocketNotifier::Read);
    QObject::connect(&input, &QSocketNotifier::activated, &app, [&input, &window] {
        char buffer[256];
        const ssize_t n = ::read(STDIN_FILENO, buffer, sizeof buffer);
        if (n < 0 && (errno == EAGAIN || errno == EINTR)) return;
        input.setEnabled(false);
        window.finish();
    });

    return app.exec();
}
