#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  Theme — the Mauvely design-system tokens, as Qt colours.
//
//  The suite ships dark and is designed dark first: a near-neutral blue-grey ground,
//  Inter at medium weight, soft radii, violet as the interactive colour and teal
//  reserved as a signal.
//
//  ── The one rule ────────────────────────────────────────────────────────────
//  Brand ramp values come from `core/brand.h`. Grounds the design system authors
//  *off* the ramps are stated once here, beside their light counterpart, and
//  nowhere else. No other file in the repo ever types a hex. `brand.h` itself is
//  byte-identical to Compose's, Snap's and the hub's — a fork there breaks smoke
//  tests in three other repos.
//
//  Copied from cloud-app-desktop/src/core/theme.{h,cpp}. Relay adds
//  `chromeDeep()` and moves the dark interactive violet one ramp step; both are
//  explained at their definitions in theme.cpp.
//
//  ── Why this differs from Compose's Theme ───────────────────────────────────
//  Compose carries five tokens (bg / surface / text / accent / divider) and
//  derives its light variant by *mirroring* ramp steps (100<->900). That was
//  enough for one app but it cannot express the design system, which specifies
//  five distinct grounds and three border tiers per theme:
//
//      bg-page      #161826   bg-page-alt   #1b1d2b   surface-card    #232532
//      surface-raised #2b2d3c  surface-sunken #12142e  surface-inset  violet 8%
//
//  Those are *authored* values, not mirrored ones, and Snap's chrome leans on
//  all of them — the title bar, sidebar, tab strip and inspector are every one
//  `bgAlt()`, editor grounds are `surfaceSunken()`, cards are `surface()` and
//  hovers are `surfaceRaised()`. So this Theme states each token for each mode
//  from the design system's own tables (`design/_ds/.../tokens/colors.css`).
//
//  The mirroring ramp accessors (`neutral`, `accentRamp`, …) are kept because
//  ported widget code picks ramp steps directly, but new code should reach for
//  the semantic tokens: they are the ones the design actually names.
// ─────────────────────────────────────────────────────────────────────────────

#include <QColor>
#include <QObject>
#include <QString>

class Theme : public QObject {
    Q_OBJECT
public:
    static Theme &instance();

    enum class Mode { Dark, Light };

    Mode mode() const { return m_mode; }
    bool isDark() const { return m_mode == Mode::Dark; }
    void setMode(Mode m);
    void toggle() { setMode(isDark() ? Mode::Light : Mode::Dark); }

    // Live UI scale (1.0 = 100%). Read by ui::font and the base app font so text
    // and metrics resize app-wide; setting it re-applies + broadcasts.
    qreal uiScale() const { return m_uiScale; }
    void setUiScale(qreal s);

    // ── An organisation's colour in place of the brand's ──────────────────
    // Not in the installer. app-base's `setBrandRamp()` needs `branding.{h,cpp}`,
    // a pinned copy the installer does not carry, and the installer runs before
    // the machine belongs to any organisation. This file is otherwise app-base's.

    /**
     * One step of the identity ramp, **not** mirrored by mode.
     *
     * `accentRamp` is the mode-aware version and is what most code wants. This
     * is for the few elements the design deliberately fixes across both themes
     * — the launcher's avatar gradient is the one, and mirroring it would turn
     * near-black initials into near-white ones on the same fill.
     *
     * Every violet in theme.cpp goes through this too, which is what makes the
     * override total rather than partial: a token that read `brand::violet()`
     * directly would stay Mauvely violet beside a customer's green.
     */
    QColor brandStep(int step) const;

    // ── Grounds ───────────────────────────────────────────────────────────
    // Five steps, not two. `bg()` is the window ground; `bgAlt()` is the chrome
    // ground (title bar, sidebar, tab strip, inspector — anything framing
    // content); `surface()` is a card; `surfaceRaised()` is a card's hover or a
    // menu; `surfaceSunken()` is a well the content sits *in* (editor canvas
    // ground, segmented-control track).
    QColor bg() const;             // --bg-page
    QColor bgAlt() const;          // --bg-page-alt
    QColor surface() const;        // --surface-card
    QColor surfaceRaised() const;  // --surface-raised
    QColor surfaceSunken() const;  // --surface-sunken
    QColor surfaceInset() const;   // --surface-inset — a violet-tinted notice fill
    /** The deepest chrome ground, below `bgAlt`: used where chrome frames the
     *  whole window rather than one region — the icon rail and the status strip.
     *  Compose calls this `statusBg()` because it had exactly one such surface;
     *  There are two, so the name says what it is rather than where it goes. */
    QColor chromeDeep() const;

    // ── Borders, in three tiers ───────────────────────────────────────────
    // `borderSubtle` separates regions, `borderHairline` separates rows inside
    // one card, `borderStrong` makes an input read as inset. The design uses all
    // three within a single view, so a lone `divider()` cannot stand in.
    QColor borderSubtle() const;
    QColor borderHairline() const;
    QColor borderStrong() const;
    /** Alias for `borderSubtle()`. Kept so widget code ported from Compose,
     *  which only knew one border, keeps compiling and lands on the right tier. */
    QColor divider() const { return borderSubtle(); }

    // ── Text, in five weights of quiet ────────────────────────────────────
    QColor text() const;       // --text-strong
    QColor textBody() const;   // --text-body
    QColor textMuted() const;  // --text-muted — the resting colour for icons
    QColor textFaint() const;  // --text-faint — eyebrows, metadata
    QColor textQuiet() const;  // --text-quiet — placeholder dots, disabled

    // ── Interactive violet ────────────────────────────────────────────────
    // `primary` fills; `primarySoft` is the tinted rest/hover state of a quiet
    // control and the fill behind an active nav row; `primaryText` is a label
    // sitting on `primarySoft`; `onPrimary` is a label sitting on `primary`.
    QColor primary() const;
    QColor primaryHover() const;
    QColor primarySoft() const;
    QColor primarySoftHover() const;
    QColor primaryText() const;
    QColor onPrimary() const;
    /** Alias for `primary()`, for ported code that spoke of one "accent". */
    QColor accent() const { return primary(); }

    // ── Mauve, the supporting accent ──────────────────────────────────────
    // Tints, avatars, editorial highlights — never the main action.
    QColor accentMauve() const;
    QColor accentMauveSoft() const;

    // ── Tonal ramps (mirrored in light mode) ──────────────────────────────
    // 100 lightest … 900 darkest on the canonical dark set. Prefer the semantic
    // tokens above; these exist for code that genuinely wants a ramp step.
    QColor neutral(int step) const;      // brand slate
    QColor accentRamp(int step) const;   // brand violet
    QColor mauveRamp(int step) const;    // brand mauve
    QColor signalRamp(int step) const;   // brand teal

    // ── Status (flat brand values; they do not mirror by mode) ────────────
    // Teal is the locked signal: live, complete, sent. At most one per screen.
    QColor signal() const;
    QColor success() const;
    QColor danger() const;
    QColor warning() const;

    // ── Overlays ──────────────────────────────────────────────────────────
    QColor scrim() const;      // dialog backdrop
    QColor focusRing() const;  // 3px ring, outside the control, never removed
    QColor selectionBg() const;
    /** Shadow ink for hand-painted elevation — mauve-black on light, near-black
     *  on dark. The design system calls this `--ink-rgb`. */
    QColor ink(qreal alpha) const;
    /** Text and icons sitting *on* an `ink()` scrim. Deliberately **not**
     *  theme-mirrored: `ink()` is dark in both themes because it overlays
     *  arbitrary image content, so a mirrored foreground turns dark-on-dark in
     *  light mode. That is exactly what happened to the capture card's format
     *  badge and the region overlay's readout, both of which vanished. */
    QColor onInk() const;

    // ── Helpers ───────────────────────────────────────────────────────────
    static QColor mix(const QColor &a, const QColor &b, qreal ratioOfA);
    static QColor withAlpha(QColor c, qreal a);
    QColor accentTint(qreal a) const { return withAlpha(primary(), a); }
    QColor textTint(qreal a) const { return withAlpha(text(), a); }

    // Font family names (registered by icons::init() at startup).
    static QString displayFamily();  // Baloo 2 — titles
    static QString bodyFamily();     // Inter — body & dense UI
    static QString monoFamily();     // JetBrains Mono — labels & code

    // Global stylesheet (scrollbars, inputs, tooltips, menus) + palette.
    QString styleSheet() const;
    void apply();  // install font, palette, and stylesheet on qApp

signals:
    void changed();

private:
    explicit Theme(QObject *parent = nullptr);

    Mode m_mode = Mode::Dark;
    qreal m_uiScale = 1.0;
};
