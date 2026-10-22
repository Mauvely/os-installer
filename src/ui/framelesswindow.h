#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  FramelessWindow — a custom-decorated top-level shell.
//
//  We drop the OS title bar entirely (Qt::FramelessWindowHint) and supply our
//  own chrome: a title bar widget across the top, a body, and a status strip.
//  Window moving and edge-resizing use Qt 6's startSystemMove / startSystemResize
//  so they behave natively on Wayland and X11 without a server-side frame.
//  A hairline frame is painted around the window when it isn't maximised.
//
//  On Windows none of the translucency runs — DWM rounds and edges the
//  window itself (DWMWA_WINDOW_CORNER_PREFERENCE, DWMWA_BORDER_COLOR), and a
//  translucent top-level is what turned an unpainted native child into a
//  hole.
//
//  Windows also keeps its *real* frame styles and only hides them. Qt's
//  Qt::FramelessWindowHint leaves a WS_POPUP behind, and Windows does not
//  treat a popup as a window: no Snap Layouts under the maximise button, no
//  drag-to-edge tiling, no Win+Arrow, and SW_MAXIMIZE sizes it to the whole
//  monitor instead of the work area — the taskbar disappears and the desktop
//  compositor stops presenting it like an ordinary window. So the native
//  window keeps WS_OVERLAPPEDWINDOW and WM_NCCALCSIZE takes the frame's
//  *pixels* away instead, which is how every custom-titlebar Windows app does
//  it. WM_NCHITTEST then hands the caption band, the resize edges and the
//  maximise button back to Windows by name.
// ─────────────────────────────────────────────────────────────────────────────

#include <QPointer>
#include <QWidget>

#include <functional>

class QVBoxLayout;
class QWindow;

class FramelessWindow : public QWidget {
    Q_OBJECT
public:
    explicit FramelessWindow(QWidget *parent = nullptr);

    // Compose the shell top-to-bottom. Each may be called once.
    void setTitleBar(QWidget *bar);
    void setBody(QWidget *body);
    void setStatusBar(QWidget *strip);

    // Called before the window closes; return false to keep it open
    // (the unsaved-changes prompt lives here).
    void setCloseGuard(std::function<bool()> guard) { m_closeGuard = std::move(guard); }

    /** Name the title bar's maximise button so Windows can own it.
     *
     *  Windows 11 opens the Snap Layouts flyout when the pointer rests on a
     *  window's maximise button, and the only way it can know where ours is
     *  is WM_NCHITTEST answering HTMAXBUTTON. That answer also takes the
     *  button's mouse events away from Qt, so `hover` is called instead —
     *  without it the button would look dead under the pointer. The click
     *  comes back as WM_NCLBUTTONUP and still runs `toggleMaximized()`.
     *
     *  A no-op on every other platform. */
    void setMaximizeButton(QWidget *button, std::function<void(bool)> hover = {});

public slots:
    void beginSystemMove();     // called by the title bar on drag
    void toggleMaximized();
    void minimizeWindow() { showMinimized(); }

signals:
    void maximizedChanged(bool maximized);

protected:
    bool event(QEvent *) override;
    bool nativeEvent(const QByteArray &type, void *message, qintptr *result) override;
    void resizeEvent(QResizeEvent *) override;
    void paintEvent(QPaintEvent *) override;
    void changeEvent(QEvent *) override;
    void closeEvent(QCloseEvent *) override;
    bool eventFilter(QObject *obj, QEvent *ev) override;
    /** Lets a widget and everything under it start an edge resize. The title
     *  bar, the body and the status strip get it when they are set; a widget
     *  that arrives in them later (a pane, a tab, the widget a web view makes
     *  once its page loads) has no edge until a subclass gives it this. */
    void installResizeFilter(QWidget *root);

private:
    /** Runs on every `WinIdChange` — the moment the platform window behind
     *  this widget is created or replaced. Qt rebuilds the platform window on
     *  some state changes, and both the frame styles and the DWM attributes
     *  go with the old one, so both are re-applied here. */
    void onNativeWindowChanged();
    /** Windows: ask DWM for rounded corners and the theme's edge colour on
     *  the current platform window. No-op elsewhere. */
    void applyWindowsChrome();
    /** Windows: put the real frame styles back on the native window, so the
     *  shell treats it as an ordinary top-level. Checks before it writes —
     *  the SWP_FRAMECHANGED it needs re-runs WM_NCCALCSIZE, and doing that on
     *  every theme change or state change would be a relayout for nothing.
     *  No-op elsewhere. */
    void ensureWindowsFrame();
    void applyRoundedMask();
    /** Keeps the corner overlay the size of the window and on top of whatever
     *  was added last. */
    void updateCornerOverlay();

    std::function<bool()> m_closeGuard;
    Qt::Edges edgeAt(const QPoint &globalPos) const;
    /** The same test against a point already in this widget's coordinates.
     *  Windows hands WM_NCHITTEST a screen point in *device* pixels, which
     *  ScreenToClient turns into a client point — a global round trip through
     *  Qt would have to undo the scaling again. */
    Qt::Edges edgeAtLocal(const QPoint &local) const;
    void updateResizeCursor(Qt::Edges edges);

    QVBoxLayout *m_layout = nullptr;
    /** Erases the four corners *after* the children have painted.
     *
     *  The window paints a rounded rect, and then every child paints a square
     *  fill straight over the corners — TitleBar's paintEvent is a plain
     *  fillRect(rect(), …). On Hyprland that never showed, because Hyprland
     *  rounds every surface itself and clipped it away. KWin does not round an
     *  undecorated window, so there the corners were simply square.
     *
     *  setMask() is not the fix: on Wayland it sets the input region and does
     *  not clip rendering at all. So the corners are cleared with
     *  CompositionMode_Clear on top of everything instead, which works because
     *  the window is WA_TranslucentBackground and the children composite into
     *  the same backing store. */
    QWidget *m_cornerOverlay = nullptr;
    QWidget *m_titleBar = nullptr;
    QWidget *m_body = nullptr;
    QWidget *m_status = nullptr;
    bool m_cursorOverridden = false;

    /** The maximise button Windows hit-tests as HTMAXBUTTON, and the hover it
     *  can no longer deliver itself. Empty everywhere else. */
    QPointer<QWidget> m_maxButton;
    std::function<void(bool)> m_maxHover;
    bool m_maxHovered = false;

    static constexpr int kResizeMargin = 6;
    static constexpr int kFrame = 1;
};
