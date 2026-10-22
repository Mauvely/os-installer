#include "ui/framelesswindow.h"

#include "core/theme.h"
#include "core/tokens.h"

#include <QApplication>
#include <QCloseEvent>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QWindow>

#ifdef Q_OS_WIN
// The window's corners and edge are DWM's on Windows — see applyWindowsChrome.
// The frame's *pixels* are ours — see nativeEvent.
#include <qt_windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <windowsx.h>
#endif

namespace {

#ifdef Q_OS_WIN
// The Windows 11 attributes, by value: the enums need a Windows 11 SDK, and
// the numbers are what an older SDK's dwmapi.h lacks. Windows 10 answers
// E_INVALIDARG to both and the window stays square and edgeless — accepted.
constexpr DWORD kDwmWindowCornerPreference = 33;  // DWMWA_WINDOW_CORNER_PREFERENCE
constexpr DWORD kDwmBorderColor = 34;             // DWMWA_BORDER_COLOR
constexpr int kDwmCornerRound = 2;                // DWMWCP_ROUND

/** What an ordinary top-level is made of, and what Qt's frameless hint takes
 *  away. WS_THICKFRAME is the one that buys Aero Snap and a maximise that
 *  stops at the work area; WS_CAPTION buys the minimise/restore animations
 *  and the taskbar's window thumbnail; the box styles let the shell offer
 *  Snap Layouts at all. None of them is ever *drawn*, because WM_NCCALCSIZE
 *  gives the client area the whole window. */
constexpr LONG_PTR kFrameStyles =
    WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;

/** How far Windows oversizes a maximised window: it hangs the frame off every
 *  edge of the work area, expecting the frame not to be part of the client.
 *  Ours is, so the same amount has to come back off. */
int zoomedFrameX() {
    return GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
}
int zoomedFrameY() {
    return GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
}

/**
 * The screen edge holding an auto-hidden taskbar on this window's monitor, or 0.
 *
 * This is a problem a custom frame has and an ordinary window does not. An
 * ordinary maximised window keeps a non-client frame, so its *client* never
 * reaches the screen edge; ours takes the whole window as client area
 * (WM_NCCALCSIZE above), and when the taskbar auto-hides the work area is the
 * whole monitor — so maximising covered the monitor exactly, pixel for pixel.
 *
 * That is the condition the desktop compositor reads as full-screen, and it
 * then hands the window a presentation path meant for games. On a
 * variable-refresh display the effect is unmistakable: the refresh rate drops
 * to a flat 60 Hz the moment the window is maximised, and the pointer starts
 * to stutter. Reported on this machine, and fixed by the pixel below.
 *
 * The same pixel is what lets an auto-hidden taskbar come back over us, for
 * the same underlying reason — the shell needs an uncovered edge to notice the
 * pointer. Chromium carries this workaround for both. The visible cost is a
 * hairline of desktop along that edge while the bar is hidden.
 */
UINT autoHideEdge(HWND hwnd) {
    APPBARDATA state{};
    state.cbSize = sizeof state;
    if (!(SHAppBarMessage(ABM_GETSTATE, &state) & ABS_AUTOHIDE)) return 0;

    MONITORINFO mi{};
    mi.cbSize = sizeof mi;
    if (!GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi))
        return 0;

    // Per monitor, not per desktop: the bar is on one screen and the other
    // screens must not lose a pixel for it.
    for (const UINT edge : {UINT(ABE_BOTTOM), UINT(ABE_TOP), UINT(ABE_LEFT),
                            UINT(ABE_RIGHT)}) {
        APPBARDATA bar{};
        bar.cbSize = sizeof bar;
        bar.uEdge = edge;
        bar.rc = mi.rcMonitor;
        if (SHAppBarMessage(ABM_GETAUTOHIDEBAREX, &bar)) return edge;
    }
    return 0;
}

/** Whether `internalWinId()` is really an HWND.
 *
 *  The offscreen platform hands out a synthetic window id, and passing that to
 *  SetWindowLongPtr/SetWindowPos is a Win32 call on a handle that names no
 *  window. The harness runs under it on this very platform, so this is not a
 *  theoretical branch. */
bool onNativeWindows() {
    return QGuiApplication::platformName() == QLatin1String("windows");
}
#endif

// Draws nothing of its own except the window's edge, and erases everything
// outside the rounded path. Lives on top of every sibling so a child's square
// background cannot survive in a corner.
class CornerOverlay : public QWidget {
public:
    explicit CornerOverlay(QWidget *parent) : QWidget(parent) {
        // Never take a click: the whole point is that it covers the window, and
        // the resize edges and the title bar's drag area are underneath it.
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setFocusPolicy(Qt::NoFocus);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        auto *win = qobject_cast<FramelessWindow *>(parentWidget());
        if (win && (win->isMaximized() || win->isFullScreen())) return;

        const qreal r = tokens::chrome::kWindowRadius;
        QPainterPath rounded;
        rounded.addRoundedRect(QRectF(rect()), r, r);

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        // Everything outside the rounded path, erased. Clear rather than
        // filling with a colour: the window is translucent, and the desktop is
        // what should show through a corner.
        QPainterPath outside;
        outside.addRect(QRectF(rect()));
        p.setCompositionMode(QPainter::CompositionMode_Clear);
        p.fillPath(outside.subtracted(rounded), Qt::black);

        // The 1px edge, drawn here rather than in the window's own paintEvent
        // for the same reason the corners are: a child would paint over it.
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        p.setPen(QPen(Theme::instance().borderStrong(), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), r, r);
    }
};

}  // namespace

FramelessWindow::FramelessWindow(QWidget *parent) : QWidget(parent) {
    setWindowFlag(Qt::Window);
    setWindowFlag(Qt::FramelessWindowHint);
    setMouseTracking(true);
    setMinimumSize(tokens::chrome::kWindowMinWidth,
                   tokens::chrome::kWindowMinHeight);
    resize(tokens::chrome::kWindowWidth, tokens::chrome::kWindowHeight);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(kFrame, kFrame, kFrame, kFrame);
    m_layout->setSpacing(0);

#ifndef Q_OS_WIN
    // The design draws the window with 12px corners, so the widget has to be
    // translucent for the desktop to show through outside the rounded path.
    // Only when not maximised: a maximised window is edge-to-edge, and rounding
    // it would leave four transparent notches over the desktop.
    setAttribute(Qt::WA_TranslucentBackground, true);

    m_cornerOverlay = new CornerOverlay(this);
    m_cornerOverlay->setGeometry(rect());
    connect(&Theme::instance(), &Theme::changed, m_cornerOverlay,
            qOverload<>(&QWidget::update));
#else
    // On Windows the window is opaque and DWM rounds and edges it. A
    // translucent top-level was a layered window copied whole on every
    // flush, and the one in which an unpainted native child (the first
    // WebEngine view) showed the desktop through the card until its first
    // frame. The edge follows the theme through the same DWM attribute.
    connect(&Theme::instance(), &Theme::changed, this,
            [this] { applyWindowsChrome(); });
#endif

    installEventFilter(this);  // catch resize hovers on the window's own frame
    connect(this, &FramelessWindow::maximizedChanged, this, [this] { update(); });
}

void FramelessWindow::setTitleBar(QWidget *bar) {
    m_titleBar = bar;
    m_layout->insertWidget(0, bar);
    installResizeFilter(bar);
    updateCornerOverlay();
}

void FramelessWindow::setBody(QWidget *body) {
    m_body = body;
    m_layout->insertWidget(m_titleBar ? 1 : 0, body, /*stretch*/ 1);
    installResizeFilter(body);
    updateCornerOverlay();
}

void FramelessWindow::setStatusBar(QWidget *strip) {
    m_status = strip;
    m_layout->addWidget(strip);
    installResizeFilter(strip);
    updateCornerOverlay();
}

// Install the resize event filter across a whole subtree so edge hovers work
// even when child widgets sit under the window border.
void FramelessWindow::installResizeFilter(QWidget *root) {
    if (!root) return;
    const QList<QWidget *> kids = root->findChildren<QWidget *>();
    root->installEventFilter(this);
    root->setMouseTracking(true);
    for (QWidget *w : kids) {
        w->installEventFilter(this);
        w->setMouseTracking(true);
    }
}

void FramelessWindow::beginSystemMove() {
    if (isMaximized()) return;
    if (QWindow *w = windowHandle()) w->startSystemMove();
}

void FramelessWindow::toggleMaximized() {
#ifdef Q_OS_WIN
    // Through Windows, not through Qt. Qt "maximises" a frameless window by
    // sizing it to the screen and never tells the shell: WS_MAXIMIZE stays off,
    // so DWM keeps rounding the corners at the screen's edges, the
    // WM_NCCALCSIZE insets above never run, an auto-hidden taskbar has no pixel
    // to rise from, and Snap Layouts sees nothing to restore. Measured on
    // 2026-09-09 by sending the title bar's HTMAXBUTTON click: after
    // showMaximized() the window sat at 0,0 2560×1440 with IsZoomed false;
    // after SW_MAXIMIZE it sat at -8,-8 2576×1456 with IsZoomed true — the
    // shape the frame code above expects, and the one a drag to the top edge
    // produces. Qt learns the state from WM_SIZE either way, so isMaximized()
    // and changeEvent() follow.
    if (onNativeWindows() && internalWinId()) {
        const HWND hwnd = reinterpret_cast<HWND>(internalWinId());
        ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
        return;
    }
#endif
    if (isMaximized())
        showNormal();
    else
        showMaximized();
}

void FramelessWindow::setMaximizeButton(QWidget *button,
                                        std::function<void(bool)> hover) {
    m_maxButton = button;
    m_maxHover = std::move(hover);
}

Qt::Edges FramelessWindow::edgeAt(const QPoint &globalPos) const {
    return edgeAtLocal(mapFromGlobal(globalPos));
}

Qt::Edges FramelessWindow::edgeAtLocal(const QPoint &p) const {
    if (!rect().contains(p)) return {};
    Qt::Edges e;
    const int M = kResizeMargin;
    if (p.x() <= M) e |= Qt::LeftEdge;
    else if (p.x() >= width() - M) e |= Qt::RightEdge;
    if (p.y() <= M) e |= Qt::TopEdge;
    else if (p.y() >= height() - M) e |= Qt::BottomEdge;
    return e;
}

void FramelessWindow::updateResizeCursor(Qt::Edges edges) {
    Qt::CursorShape shape = Qt::ArrowCursor;
    if ((edges & Qt::LeftEdge && edges & Qt::TopEdge) ||
        (edges & Qt::RightEdge && edges & Qt::BottomEdge))
        shape = Qt::SizeFDiagCursor;
    else if ((edges & Qt::RightEdge && edges & Qt::TopEdge) ||
             (edges & Qt::LeftEdge && edges & Qt::BottomEdge))
        shape = Qt::SizeBDiagCursor;
    else if (edges & (Qt::LeftEdge | Qt::RightEdge))
        shape = Qt::SizeHorCursor;
    else if (edges & (Qt::TopEdge | Qt::BottomEdge))
        shape = Qt::SizeVerCursor;

    if (shape == Qt::ArrowCursor) {
        if (m_cursorOverridden) {
            QApplication::restoreOverrideCursor();
            m_cursorOverridden = false;
        }
    } else {
        if (m_cursorOverridden) QApplication::restoreOverrideCursor();
        QApplication::setOverrideCursor(shape);
        m_cursorOverridden = true;
    }
}

bool FramelessWindow::event(QEvent *ev) {
    if (ev->type() == QEvent::WinIdChange) onNativeWindowChanged();
    return QWidget::event(ev);
}

bool FramelessWindow::nativeEvent(const QByteArray &type, void *message,
                                  qintptr *result) {
#ifdef Q_OS_WIN
    if (type != QByteArrayLiteral("windows_generic_MSG"))
        return QWidget::nativeEvent(type, message, result);

    auto *msg = static_cast<MSG *>(message);
    switch (msg->message) {
    case WM_NCCALCSIZE: {
        // The whole window becomes the client area, so the frame styles above
        // exist without a single frame pixel being drawn. wParam FALSE is the
        // rectangle-only form and needs no answer from us.
        if (!msg->wParam) break;
        auto *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);
        RECT &rc = params->rgrc[0];
        if (IsZoomed(msg->hwnd)) {
            // A maximised window is positioned frame-width outside the work
            // area on every side. Left alone, that much of the content would
            // sit off-screen — and over the taskbar.
            const int fx = zoomedFrameX();
            const int fy = zoomedFrameY();
            rc.left += fx;
            rc.right -= fx;
            rc.top += fy;
            rc.bottom -= fy;

            // Leave the auto-hidden taskbar its pixel — see autoHideEdge.
            switch (autoHideEdge(msg->hwnd)) {
            case ABE_LEFT: rc.left += 1; break;
            case ABE_TOP: rc.top += 1; break;
            case ABE_RIGHT: rc.right -= 1; break;
            case ABE_BOTTOM: rc.bottom -= 1; break;
            default: break;
            }
        }
        *result = 0;
        return true;
    }
    case WM_SETTINGCHANGE:
    case WM_DISPLAYCHANGE:
        // The taskbar can be moved, switched to auto-hide, or taken off this
        // monitor while the window sits maximised, and nothing re-asks for the
        // frame on its own. Ask, so the pixel above appears and disappears with
        // the bar rather than at the next maximise.
        if (IsZoomed(msg->hwnd))
            SetWindowPos(msg->hwnd, nullptr, 0, 0, 0, 0,
                         SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER
                             | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
        break;
    case WM_NCHITTEST: {
        // Screen pixels → client pixels → Qt's own coordinates. The client
        // rect is the widget's rect (WM_NCCALCSIZE above), so the only
        // conversion left is this window's device pixel ratio.
        POINT pt{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
        ScreenToClient(msg->hwnd, &pt);
        const qreal dpr = devicePixelRatioF() > 0 ? devicePixelRatioF() : 1.0;
        const QPoint local(int(pt.x / dpr), int(pt.y / dpr));

        if (!isMaximized() && !isFullScreen()) {
            const Qt::Edges e = edgeAtLocal(local);
            const bool l = e.testFlag(Qt::LeftEdge), r = e.testFlag(Qt::RightEdge);
            const bool t = e.testFlag(Qt::TopEdge), b = e.testFlag(Qt::BottomEdge);
            int ht = 0;
            if (l && t) ht = HTTOPLEFT;
            else if (r && t) ht = HTTOPRIGHT;
            else if (l && b) ht = HTBOTTOMLEFT;
            else if (r && b) ht = HTBOTTOMRIGHT;
            else if (l) ht = HTLEFT;
            else if (r) ht = HTRIGHT;
            else if (t) ht = HTTOP;
            else if (b) ht = HTBOTTOM;
            if (ht) {
                *result = ht;
                return true;
            }
        }

        // Claimed even while maximised: Snap Layouts offers to un-maximise
        // into a zone, which is exactly what the button is for.
        if (m_maxButton && m_maxButton->isVisible()
            && m_maxButton->rect().contains(m_maxButton->mapFrom(this, local))) {
            *result = HTMAXBUTTON;
            return true;
        }

        // Bare title bar — a point over one of its children belongs to Qt, or
        // no control in the title bar could be clicked.
        if (m_titleBar && m_titleBar->isVisible()) {
            const QPoint inBar = m_titleBar->mapFrom(this, local);
            if (m_titleBar->rect().contains(inBar) && !m_titleBar->childAt(inBar)) {
                *result = HTCAPTION;
                return true;
            }
        }

        *result = HTCLIENT;
        return true;
    }
    case WM_NCMOUSEMOVE:
        if (msg->wParam == HTMAXBUTTON) {
            if (!m_maxHovered) {
                m_maxHovered = true;
                if (m_maxHover) m_maxHover(true);
                // The only way a non-client hover ever ends: ask to be told.
                TRACKMOUSEEVENT tme{sizeof(TRACKMOUSEEVENT),
                                    TME_LEAVE | TME_NONCLIENT, msg->hwnd, 0};
                TrackMouseEvent(&tme);
            }
            *result = 0;
            return true;
        }
        break;
    case WM_NCMOUSELEAVE:
        if (m_maxHovered) {
            m_maxHovered = false;
            if (m_maxHover) m_maxHover(false);
        }
        break;
    case WM_NCLBUTTONDOWN:
        // Swallowed so DefWindowProc does not run its own caption-button
        // tracking against a caption that is not there.
        if (msg->wParam == HTMAXBUTTON) {
            *result = 0;
            return true;
        }
        break;
    case WM_NCLBUTTONUP:
        if (msg->wParam == HTMAXBUTTON) {
            if (m_maxHovered) {
                m_maxHovered = false;
                if (m_maxHover) m_maxHover(false);
            }
            toggleMaximized();
            *result = 0;
            return true;
        }
        break;
    default:
        break;
    }
#endif
    return QWidget::nativeEvent(type, message, result);
}

void FramelessWindow::applyWindowsChrome() {
#ifdef Q_OS_WIN
    if (!onNativeWindows() || !internalWinId()) return;
    const HWND hwnd = reinterpret_cast<HWND>(internalWinId());
    const int corner = kDwmCornerRound;
    DwmSetWindowAttribute(hwnd, kDwmWindowCornerPreference, &corner, sizeof corner);
    const QColor c = Theme::instance().borderStrong();
    const COLORREF edge = RGB(c.red(), c.green(), c.blue());
    DwmSetWindowAttribute(hwnd, kDwmBorderColor, &edge, sizeof edge);
#endif
}

void FramelessWindow::ensureWindowsFrame() {
#ifdef Q_OS_WIN
    if (!onNativeWindows() || !internalWinId()) return;
    const HWND hwnd = reinterpret_cast<HWND>(internalWinId());
    const LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    // WS_POPUP is what Qt leaves behind for a frameless window, and what stops
    // the shell treating this as a window at all.
    const LONG_PTR wanted = (style & ~LONG_PTR(WS_POPUP)) | kFrameStyles;
    if (style == wanted) return;
    SetWindowLongPtr(hwnd, GWL_STYLE, wanted);
    // Nothing takes effect until the frame is recalculated, and that is the
    // call that sends the first WM_NCCALCSIZE.
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER
                     | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
#endif
}

void FramelessWindow::onNativeWindowChanged() {
    // Every new platform window gets the DWM chrome again — Qt recreates
    // the window on some state changes, and the attributes go with the old one.
    // The frame styles go with it too.
    ensureWindowsFrame();
    applyWindowsChrome();
}

bool FramelessWindow::eventFilter(QObject *obj, QEvent *ev) {
#ifdef Q_OS_WIN
    // Windows answers for the edges in WM_NCHITTEST now, and its own drag is
    // the one Aero Snap watches. Testing them here as well would consume the
    // press before the shell ever saw it, and set a second cursor over the
    // one Windows already sets for HTLEFT and friends.
    return QWidget::eventFilter(obj, ev);
#else
    if (isMaximized() || isFullScreen()) {
        if (m_cursorOverridden) {
            QApplication::restoreOverrideCursor();
            m_cursorOverridden = false;
        }
        return QWidget::eventFilter(obj, ev);
    }

    switch (ev->type()) {
    case QEvent::MouseMove: {
        auto *me = static_cast<QMouseEvent *>(ev);
        if (me->buttons() == Qt::NoButton)
            updateResizeCursor(edgeAt(me->globalPosition().toPoint()));
        break;
    }
    case QEvent::MouseButtonPress: {
        auto *me = static_cast<QMouseEvent *>(ev);
        if (me->button() == Qt::LeftButton) {
            const Qt::Edges edges = edgeAt(me->globalPosition().toPoint());
            if (edges) {
                if (m_cursorOverridden) {
                    QApplication::restoreOverrideCursor();
                    m_cursorOverridden = false;
                }
                if (QWindow *w = windowHandle()) {
                    w->startSystemResize(edges);
                    return true;  // consume; don't let a child react
                }
            }
        }
        break;
    }
    case QEvent::Leave:
        if (obj == this && m_cursorOverridden) {
            QApplication::restoreOverrideCursor();
            m_cursorOverridden = false;
        }
        break;
    default:
        break;
    }
    return QWidget::eventFilter(obj, ev);
#endif
}

void FramelessWindow::closeEvent(QCloseEvent *ev) {
    if (m_closeGuard && !m_closeGuard()) {
        ev->ignore();
        return;
    }
    ev->accept();
}

void FramelessWindow::changeEvent(QEvent *ev) {
    if (ev->type() == QEvent::WindowStateChange) {
        const bool maxed = isMaximized() || isFullScreen();
        const int m = maxed ? 0 : kFrame;
        if (m_layout) m_layout->setContentsMargins(m, m, m, m);
        // Maximising must drop the rounded mask, and restoring must put it back;
        // a state change does not always come with a resize.
        applyRoundedMask();
        emit maximizedChanged(maxed);
    }
    QWidget::changeEvent(ev);
}

void FramelessWindow::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    Theme &t = Theme::instance();

    const bool edgeToEdge = isMaximized() || isFullScreen();
#ifdef Q_OS_WIN
    // Always square: DWM clips the corners itself, and rounding a rect DWM
    // also rounds leaves notches.
    const bool square = true;
#else
    const bool square = edgeToEdge;
#endif
    if (square) {
        // Square when maximised: rounding an edge-to-edge window would punch
        // four transparent notches through to the desktop.
        Q_UNUSED(edgeToEdge);
        p.fillRect(rect(), t.bg());
        return;
    }

    // Fill only. The rounded edge and the corner cut-outs are the corner
    // overlay's job, because they have to happen after the children paint.
    const qreal r = tokens::chrome::kWindowRadius;
    p.setPen(Qt::NoPen);
    p.setBrush(t.bg());
    p.drawRoundedRect(QRectF(rect()), r, r);
}

// Children are square, so a rounded window would still show their corners
// poking out over the transparent background. Masking the whole widget to the
// same rounded path clips them to it.
void FramelessWindow::resizeEvent(QResizeEvent *ev) {
    QWidget::resizeEvent(ev);
    applyRoundedMask();
    updateCornerOverlay();
}

void FramelessWindow::updateCornerOverlay() {
    if (!m_cornerOverlay) return;
    m_cornerOverlay->setGeometry(rect());
    // Raised on every resize rather than once at construction: setTitleBar(),
    // setBody() and setStatusBar() each add a child after this was created, and
    // a later sibling would otherwise paint over it.
    m_cornerOverlay->raise();
    m_cornerOverlay->update();
}

void FramelessWindow::applyRoundedMask() {
#ifdef Q_OS_WIN
    // A region mask on a DWM-rounded window gives jagged corners and fights
    // DWM's own clip.
    return;
#endif
    if (isMaximized() || isFullScreen()) {
        clearMask();
        return;
    }
    // A bitmap mask has hard edges, so the antialiased border painted above does
    // the smoothing and the mask is inset by a pixel to stay behind it.
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()), tokens::chrome::kWindowRadius,
                        tokens::chrome::kWindowRadius);
    setMask(QRegion(path.toFillPolygon().toPolygon()));
}
