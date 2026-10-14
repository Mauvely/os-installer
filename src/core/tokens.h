#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  tokens — the non-colour half of the design system, as compile-time constants.
//
//  Colour lives in `brand.h` and `theme.h`. Everything else the design system
//  names — spacing, radii, type sizes, control heights, motion — lives here, and
//  this is the only place those numbers should appear.
//
//  Compose declares `brand::radius::` and then hardcodes 6 / 8 / 14 at every
//  paint site anyway, and has no spacing or type scale at all. That is the gap
//  this file closes: `space::`, `radius::`, `type::` and `motion::` are meant to
//  be *used*, so a design tweak is one edit rather than a grep.
//
//  Values mirror design/_ds/…/tokens/{spacing,radii,typography,motion,layout}.css
//  verbatim. The token name is the px value wherever the source did that, so the
//  design system's numbers survive the trip into C++ unmangled.
// ─────────────────────────────────────────────────────────────────────────────

#include <QEasingCurve>
#include <QPointF>

namespace tokens {

// ── Spacing ──────────────────────────────────────────────────────────────────
// Deliberately not a 4pt grid: the brand board uses 6 / 9 / 10 / 22 / 44 freely.
namespace space {
constexpr int k2 = 2;    constexpr int k4 = 4;    constexpr int k6 = 6;
constexpr int k8 = 8;    constexpr int k9 = 9;    constexpr int k10 = 10;
constexpr int k12 = 12;  constexpr int k14 = 14;  constexpr int k16 = 16;
constexpr int k18 = 18;  constexpr int k20 = 20;  constexpr int k22 = 22;
constexpr int k24 = 24;  constexpr int k26 = 26;  constexpr int k28 = 28;
constexpr int k32 = 32;  constexpr int k40 = 40;  constexpr int k44 = 44;
constexpr int k48 = 48;

// Semantic gaps, from the spacing token file's own aliases.
constexpr int kStack = 20;   // between stacked panels
constexpr int kGrid = 16;    // between tiles in a grid
constexpr int kInline = 12;  // between controls on one row
constexpr int kTight = 6;
}  // namespace space

// ── Radii ────────────────────────────────────────────────────────────────────
// The ladder mirrors design/_ds/…/tokens/radii.css and Compose's
// `brand::radius::` byte for byte, and is left alone so the two repos keep
// agreeing about what "lg" means.
//
// The *aliases* below are Snap's own, and they are deliberately tighter than the
// design bundle's `--radius-card: var(--radius-xl)` / `--radius-panel:
// var(--radius-2xl)`. Rendered at 1440×900 against Compose the bundle's values
// read as visibly softer than the sibling app — 16px tiles and 22px dialogs next
// to Compose's 8s and 14s is the single loudest "this is a different product"
// signal in the chrome. Compose wins that argument; see docs/architecture.md.
//
// Only these aliases and `kPill` should appear at a paint site. A bare `kMd` in
// widget code is the Compose anti-pattern this file exists to prevent.
namespace radius {
constexpr int kXs = 6;
constexpr int kSm = 8;
constexpr int kMd = 10;
constexpr int kLg = 12;
constexpr int kXl = 16;
constexpr int k2Xl = 22;
constexpr int k3Xl = 28;
constexpr int kPill = 999;

constexpr int kPanel = 14;      // dialogs, big overlays — Compose's modal.cpp:39
constexpr int kCard = kMd;      // library cards, inner tiles
constexpr int kControl = kSm;   // buttons, selects, inputs, tool buttons
constexpr int kChrome = kXs;    // title-bar, window and icon buttons, menu items
constexpr int kThumb = kXs;     // media inset *inside* a card
constexpr int kSwatch = kSm;
}  // namespace radius

// ── Type ─────────────────────────────────────────────────────────────────────
// Never set Inter above 18px and never set Baloo 2 below 15px — the two faces
// swap roles rather than overlapping.
namespace type {
constexpr int kMicro = 9;
constexpr int k2Xs = 11;
constexpr int kXs = 12;
constexpr int kSm = 13;
constexpr int kMd = 15;
constexpr int kBase = 16;
constexpr int kLg = 18;
constexpr int kXl = 22;
constexpr int k2Xl = 28;
constexpr int k3Xl = 40;

// Weights, as QFont::Weight-compatible ints.
constexpr int kRegular = 400;
constexpr int kMedium = 500;
constexpr int kSemiBold = 600;
constexpr int kDisplay = 800;  // Baloo 2 ExtraBold — every title

// Tracking, as QFont::PercentageSpacing values (100 == normal).
// The design system states these in em: 0.16em on eyebrow labels, -0.005em on
// display text. Percentage spacing is relative to the advance width, so these
// are the empirical equivalents Compose already settled on.
constexpr int kLabelTracking = 116;    // ~0.16em, uppercase mono eyebrows
constexpr int kDisplayTracking = 99;   // ~-0.005em, Baloo 2 titles

// Line heights, as multipliers.
constexpr qreal kLhTight = 1.02;    // display
constexpr qreal kLhSnug = 1.28;     // subtitles
constexpr qreal kLhNormal = 1.5;
constexpr qreal kLhRelaxed = 1.65;  // body prose
}  // namespace type

// ── Control heights ──────────────────────────────────────────────────────────
namespace control {
constexpr int kSm = 32;
constexpr int kMd = 40;
constexpr int kLg = 48;
}  // namespace control

// ── Chrome geometry ──────────────────────────────────────────────────────────
// The one source of truth for THIS app's frame — the one section of this file
// that is legitimately per-app. Every number is measured off the
// design bundle ("Mauvely Chat App Mockups.dc.html", screens 1d/1e/1f); see
// docs/architecture.md for the annotated breakdown.
//
// A measurement belongs here when two widgets have to agree about it. The
// splitter gutter is here because the conversation column and the chat pane both
// need it; a card's inner padding is not, because only that card cares.
namespace chrome {
// Title bar and status strip match Compose and Snap exactly. The bundle draws
// them at 44/26 too, which is the happy case — a chrome band a few pixels taller
// than the sibling app's is felt immediately when switching between them, and
// switching between them is the point.
constexpr int kTitleBar = 44;
constexpr int kStatusStrip = 26;

/** The icon rail: five 56px stacked icon+label items over a `chromeDeep`
 *  ground, with a 44px settings item pinned to the bottom. */
constexpr int kIconRail = 72;
constexpr int kRailItem = 56;
constexpr int kRailFooterItem = 44;
/** The teal marker beside the active rail item. */
constexpr int kRailMarkerW = 2;
constexpr int kRailMarkerH = 24;

/** The conversation list column, and the Recent/Unread/Channels strip on top
 *  of it. Rows are 60px, or 44px when Settings ▸ Compact rows is on — which is
 *  also the touch-target floor, so compact mode is forced off under
 *  platform::touchMode(). */
constexpr int kListColumn = 286;
constexpr int kListTabStrip = 38;
constexpr int kListRow = 60;
constexpr int kListRowCompact = 44;

/** The conversation header above the transcript. */
constexpr int kChatHeader = 52;
/** The thread pane, docked right — where Compose's ReviewPanel sits. */
constexpr int kThreadPane = 320;
/** The bundle draws a 3px band of window ground between the list column and the
 *  chat pane. It is a splitter handle, not a margin. */
constexpr int kSplitter = 3;

/** The widest the chat column gets, header and composer included. Past this the
 *  column centres and the surplus becomes margin.
 *
 *  It is 1000 because that is what the chat pane measures in the design bundle
 *  (1360 window − 72 rail − 286 list − 3 splitter ≈ 999), so a maximised window
 *  shows the same proportions the design was drawn at rather than the same
 *  proportions stretched. Left to fill, the messages sit at their 700px reading
 *  measure against the left edge while the composer stretches past them, and the
 *  two stop looking like parts of one conversation. */
constexpr int kChatColumnMax = 1000;

// The bundle's reference frame. Not a maximum — the transcript reflows — but
// the size every screenshot in docs/ is grabbed at, so a comparison against the
// design is like for like.
//
// The minimum is what a tiling compositor may actually grant. Qt refuses to
// shrink a window below it, and Hyprland-class compositors then *scale* the
// too-large buffer into the tile — the whole UI squashed and blurred until the
// split is dragged past the minimum. A half of 1920 is 960 and a half of 1600
// is 800, so the floor sits at 800: cramped is a layout to improve, scaled is
// a bug.
constexpr int kWindowMinWidth = 800;
constexpr int kWindowMinHeight = 560;
constexpr int kWindowWidth = 1360;
constexpr int kWindowHeight = 864;
constexpr int kWindowRadius = 12;
}  // namespace chrome

// ── Motion ───────────────────────────────────────────────────────────────────
// Short and soft. No bounce in chrome — `kEaseSpring` exists only for learner
// reward moments, which Snap does not have, so it is deliberately absent here.
namespace motion {
constexpr int kInstant = 80;
constexpr int kFast = 120;   // hover / press colour
constexpr int kBase = 180;   // reveals, tab switches
constexpr int kSlow = 280;   // dialogs, drawers
constexpr int kPage = 420;

/** Whether chrome should animate at all. Reads settingskey::Animations.
 *
 *  Everything that animates asks this rather than reading the setting itself, so
 *  "off" means off everywhere and there is one place to look when it does not.
 *  Defined in tokens.cpp to keep this header free of QSettings. */
bool animationsEnabled();

/** `ms` if motion is on, 0 if it is not. The call sites then need no branch:
 *  a QVariantAnimation with a zero duration jumps straight to its end value,
 *  which is exactly the reduced-motion behaviour the design system asks for
 *  ("drop transform, keep opacity"). */
int duration(int ms);

/** The design system's `--ease-out`, cubic-bezier(.22,.61,.36,1). Qt has no
 *  built-in cubic-bezier curve, so build a custom one. */
inline QEasingCurve easeOut() {
    QEasingCurve c(QEasingCurve::BezierSpline);
    c.addCubicBezierSegment(QPointF(0.22, 0.61), QPointF(0.36, 1.0),
                            QPointF(1.0, 1.0));
    return c;
}
}  // namespace motion

}  // namespace tokens
