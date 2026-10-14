// ─────────────────────────────────────────────────────────────────────────────
//  mauvely-installer — writes Mauvely OS to a disk, from the install medium.
//
//  It runs as root, full-screen in cage, on the live system the ISO boots
//  (image/mauvely-installer.service), and nowhere else: it refuses to start
//  without `rd.live.image` on the kernel command line. The tests drive the same
//  window with a fake runner instead (tests/tst_flow.cpp).
// ─────────────────────────────────────────────────────────────────────────────

#include "core/config.h"
#include "core/host.h"
#include "core/runner.h"
#include "core/theme.h"
#include "ui/style.h"
#include "ui/window.h"

#include <QApplication>

#include <cstdio>

int main(int argc, char *argv[]) {
    const Config config;
    if (!host::isLiveSystem(config)) {
        std::fprintf(stderr,
                     "mauvely-installer erases a disk, so it runs only on the Mauvely OS install "
                     "medium (rd.live.image is not on the kernel command line).\n");
        return 1;
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("mauvely-installer"));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));

    style::loadFonts();
    Theme::instance().apply();
    app.setStyleSheet(style::sheet());

    ProcessRunner runner;
    ui::Window window(runner, config);
    window.showFullScreen();
    return app.exec();
}
