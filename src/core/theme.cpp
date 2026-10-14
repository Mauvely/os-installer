#include "core/theme.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFont>
#include <QPalette>

#include "core/brand.h"
#include "core/tokens.h"

namespace {

int mirror(int step) { return 1000 - step; }  // 100<->900, 300<->700, 500 fixed

// Pick between the dark and light value of a token. Every semantic accessor
// below goes through this, so the two design-system tables stay legible side by
// side instead of being buried in scattered ternaries.
inline QColor pick(bool dark, const QColor &onDark, const QColor &onLight) {
    return dark ? onDark : onLight;
}

QColor hex(const char *s) { return QColor(QLatin1String(s)); }

}  // namespace

Theme &Theme::instance() {
    static Theme t;
    return t;
}

Theme::Theme(QObject *parent) : QObject(parent) {}

void Theme::setMode(Mode m) {
    if (m == m_mode) return;
    m_mode = m;
    apply();
    emit changed();
}

void Theme::setUiScale(qreal s) {
    s = qBound(0.8, s, 1.6);
    if (qAbs(s - m_uiScale) < 0.001) return;
    m_uiScale = s;
    apply();  // rescales the base app font + stylesheet
    emit changed();
}

// ── The identity ramp ────────────────────────────────────────────────────────
// app-base routes every violet through here so an organisation's ramp can
// replace it. The installer has no organisation, so this is Mauvely violet.
QColor Theme::brandStep(int step) const { return brand::violet().step(step); }

// ── Grounds ──────────────────────────────────────────────────────────────────
// Six of them, and both columns must keep all six distinct and in order. Dark
// is the design bundle's authored table. Light is Relay's own ladder, and the
// reason it is not the design system's light column is worth stating.
//
// The system's light column maps surface *and* surfaceRaised to cardSurface,
// and bgAlt *and* surfaceSunken to slate 100. Six accessors, three values. The
// separation that makes dark legible collapses, which is why light read flat.
// The visible half of that is surface == surfaceRaised: a tooltip, a menu and a
// popover are painted with the raised ground and had nowhere to go above a card.
//
// The other half is latent and worse. Light's surfaceSunken came out *lighter*
// than bg, so a "well" was a bump. Nothing renders with it today — all four
// ui::Input instances pass a tone, and the composer's QPlainTextEdit paints
// itself transparent — but it is ui::Input's **default** tone and the ground
// the global stylesheet gives every bare QLineEdit, so the first input added
// without a deliberate setTone() would have inherited an inverted one. The
// Security page is that input.
//
// The ramp cannot supply the fix. Its light end is slate 50 (#f8f9fd) and 100
// (#f3f5fe), 0.6% apart, and 200 (#e4e7f5) which collides with pageGround. So
// three values are authored here, exactly as the five dark grounds already are
// — CLAUDE.md sanctions that ("the grounds the design system authors off-ramp
// are stated once in core/theme.cpp beside their light counterpart") and
// chromeDeep() is the standing precedent.
//
// They are also one hue family. The old column mixed warm-violet greys
// (pageGround, chromeDeep) with the cool blue-white slate ramp, and adjacent
// grounds in different families read muddy.
//
// Pure white is reserved for surfaceRaised — the thing that floats. Cards and
// chrome sit one step below it, which is what gives a menu somewhere to go.
QColor Theme::bg() const {
    return pick(isDark(), brand::appGround(), brand::pageGround());
}
QColor Theme::bgAlt() const {
    return pick(isDark(), hex("#1b1d2b"), hex("#f3f1f7"));
}
QColor Theme::surface() const {
    return pick(isDark(), hex("#232532"), hex("#faf8fc"));
}
QColor Theme::surfaceRaised() const {
    return pick(isDark(), hex("#2b2d3c"), brand::cardSurface());
}
QColor Theme::surfaceSunken() const {
    return pick(isDark(), hex("#12142e"), hex("#e6e3ec"));
}
QColor Theme::chromeDeep() const {
    // Authored, not on a ramp — the same pair Compose carries as statusBg().
    // Deliberately *not* surfaceSunken(): that is #12142e, indigo 950, a
    // different hue that reads blue beside the rail's slate neighbours.
    return pick(isDark(), hex("#12141d"), hex("#e0dee6"));
}
QColor Theme::surfaceInset() const {
    // A violet wash on dark rather than a grey step, so notices read as part of
    // the brand rather than as a disabled area.
    return pick(isDark(), withAlpha(brandStep(500), 0.08),
                brand::mauve().step(50));
}

// ── Borders ──────────────────────────────────────────────────────────────────
// Light's borders are a step stronger than they were, because the two themes
// express elevation differently and should. Dark steps the ground: it has room
// below #161826 and uses it. Light has a ceiling at white and cannot step up,
// so it leans on edges and shadow instead. Matching the two exactly is what
// made them read as one design in two colours.
QColor Theme::borderSubtle() const {
    return pick(isDark(), withAlpha(hex("#e9e9ed"), 0.08),
                withAlpha(hex("#281c2d"), 0.09));
}
QColor Theme::borderHairline() const {
    return pick(isDark(), withAlpha(hex("#e9e9ed"), 0.05),
                withAlpha(hex("#281c2d"), 0.055));
}
QColor Theme::borderStrong() const {
    return pick(isDark(), withAlpha(hex("#e9e9ed"), 0.18),
                brand::slate().step(300));
}

// ── Text ─────────────────────────────────────────────────────────────────────
QColor Theme::text() const {
    return pick(isDark(), hex("#e9e9ed"), brand::textStrong());
}
QColor Theme::textBody() const {
    return pick(isDark(), brand::slate().step(300), brand::textBody());
}
QColor Theme::textMuted() const {
    return pick(isDark(), brand::slate().step(500), brand::textMuted());
}
QColor Theme::textFaint() const {
    return pick(isDark(), brand::slate().step(600), brand::textFaint());
}
QColor Theme::textQuiet() const {
    return pick(isDark(), brand::slate().step(700), brand::slate().step(400));
}

// ── Interactive violet ───────────────────────────────────────────────────────
// Relay's dark column differs from Snap's and the hub's, deliberately.
//
// Those apps step *up* the ramp on dark (violet 400, #b5abfc) so a *filled*
// primary button clears 4.5:1 against deep violet ink. Relay's design has no
// filled primary anywhere in its chrome: "New chat", "Send", "Attach 1 file",
// "Open in Compose" and the active rail item are all a 1px outline over a 12–16%
// tint, so the label sits on the window ground rather than on a violet fill.
// There, violet 500 (#968ae0) reaches ~5.9:1 and reads as the brand; violet 400
// reads as a pastel wash of it.
//
// primarySoft() moves with primary() rather than staying on 400 because every
// tint the design bundle authors is a violet-500 alpha — leaving the fill on 400
// while the border sits on 500 makes the two visibly different hues at 14%.
//
// Light is untouched, and no call site changes: everything still asks for
// Theme::primary().
QColor Theme::primary() const {
    return pick(isDark(), brandStep(500), brandStep(600));
}
QColor Theme::primaryHover() const {
    return pick(isDark(), brandStep(400), brandStep(700));
}
QColor Theme::primarySoft() const {
    return pick(isDark(), withAlpha(brandStep(500), 0.14),
                brandStep(100));
}
QColor Theme::primarySoftHover() const {
    return pick(isDark(), withAlpha(brandStep(500), 0.22),
                brandStep(200));
}
// Violet 300 (#d2cefd): rail labels under an active icon, @mention pills, the
// "3 replies" thread row. One step lighter than primary() because each of those
// sits *on* a primarySoft() tint rather than on the ground.
QColor Theme::primaryText() const {
    return pick(isDark(), brandStep(300), brandStep(700));
}
QColor Theme::onPrimary() const {
    return pick(isDark(), brandStep(950), brand::cardSurface());
}

// ── Mauve ────────────────────────────────────────────────────────────────────
QColor Theme::accentMauve() const {
    return pick(isDark(), brand::mauve().step(400), brand::accent());
}
QColor Theme::accentMauveSoft() const {
    return pick(isDark(), withAlpha(brand::mauve().step(300), 0.16),
                brand::mauve().step(100));
}

// ── Ramps (mirror the step in light mode) ────────────────────────────────────
QColor Theme::neutral(int step) const {
    return brand::slate().step(isDark() ? step : mirror(step));
}
QColor Theme::accentRamp(int step) const {
    return brandStep(isDark() ? step : mirror(step));
}
QColor Theme::mauveRamp(int step) const {
    return brand::mauve().step(isDark() ? step : mirror(step));
}
QColor Theme::signalRamp(int step) const {
    return brand::teal().step(isDark() ? step : mirror(step));
}

// ── Status ───────────────────────────────────────────────────────────────────
// Flat brand values — a warning must not become a different hue in light mode.
// Teal is nudged lighter on the dark ground so it keeps its glow.
QColor Theme::signal() const {
    return pick(isDark(), brand::teal().step(400), brand::signal());
}
QColor Theme::success() const {
    return pick(isDark(), brand::teal().step(400), brand::success());
}
QColor Theme::danger() const {
    return pick(isDark(), brand::danger().lighter(112), brand::danger());
}
QColor Theme::warning() const {
    return pick(isDark(), brand::warning(), brand::warningHover());
}

// ── Overlays ─────────────────────────────────────────────────────────────────
QColor Theme::scrim() const {
    return pick(isDark(), withAlpha(hex("#080610"), 0.60),
                withAlpha(hex("#281c2d"), 0.44));
}
QColor Theme::focusRing() const {
    return pick(isDark(), withAlpha(brandStep(400), 0.55),
                withAlpha(brandStep(600), 0.45));
}
QColor Theme::selectionBg() const {
    return pick(isDark(), withAlpha(brandStep(500), 0.35),
                brandStep(200));
}
QColor Theme::ink(qreal alpha) const {
    return withAlpha(pick(isDark(), hex("#080610"), hex("#281c2d")), alpha);
}

QColor Theme::onInk() const {
    // One value, both themes — see the header for why mirroring this is a bug.
    return brand::slate().step(200);  // #e4e7f5
}

// ── Helpers ──────────────────────────────────────────────────────────────────
QColor Theme::mix(const QColor &a, const QColor &b, qreal ra) {
    ra = qBound(0.0, ra, 1.0);
    const qreal rb = 1.0 - ra;
    return QColor::fromRgbF(a.redF() * ra + b.redF() * rb,
                            a.greenF() * ra + b.greenF() * rb,
                            a.blueF() * ra + b.blueF() * rb,
                            a.alphaF() * ra + b.alphaF() * rb);
}

QColor Theme::withAlpha(QColor c, qreal a) {
    c.setAlphaF(qBound(0.0, a, 1.0));
    return c;
}

QString Theme::displayFamily() { return brand::displayFamily(); }
QString Theme::bodyFamily()    { return brand::bodyFamily(); }
QString Theme::monoFamily()    { return brand::monoFamily(); }

// CSS rgba() string for a QColor. Qt QSS parses rgba() reliably; #AARRGGBB is
// not portable there.
static QString rgba(const QColor &c) {
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(c.red()).arg(c.green()).arg(c.blue())
        .arg(QString::number(c.alphaF(), 'f', 3));
}

// ── Global stylesheet + palette ──────────────────────────────────────────────
QString Theme::styleSheet() const {
    QString s = R"(
QToolTip {
    background: @raised@; color: @text@; border: 1px solid @border@;
    padding: 5px 9px; border-radius: @rChrome@px; font-size: 12px;
}

/* Inputs read as inset: a real border, not a hairline. */
QLineEdit, QPlainTextEdit, QTextEdit, QSpinBox, QDoubleSpinBox {
    background: @sunken@; color: @text@;
    border: 1px solid @borderStrong@; border-radius: @rControl@px;
    selection-background-color: @sel@; selection-color: @text@;
    padding: 5px 10px;
}
QLineEdit:hover, QPlainTextEdit:hover, QTextEdit:hover { border-color: @muted@; }
QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus,
QSpinBox:focus, QDoubleSpinBox:focus { border-color: @primary@; }
/* Mono is opt-in, via `setProperty("mono", true)`. Snap and the hub apply it to
   every QPlainTextEdit because theirs are all code or log views. Relay's most
   important one is the message composer, which is prose — and prose set in
   JetBrains Mono is immediately, obviously wrong. */
QPlainTextEdit[mono="true"], QTextEdit[mono="true"] { font-family: "@mono@"; }

/* Thin themed scrollbars */
QScrollBar:vertical   { background: transparent; width: 11px; margin: 2px; }
QScrollBar:horizontal { background: transparent; height: 11px; margin: 2px; }
QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
    background: @thumb@; border-radius: 5px; min-height: 30px; min-width: 30px;
}
QScrollBar::handle:hover { background: @thumbHover@; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

/* Geometry from the design bundle's attachment menu (screen 1j): a 268px card
   on `surface`, not `surfaceRaised` — a menu here floats over the composer,
   which is itself `surface`, and the raised step reads as a second elevation
   the design doesn't have. 5px padding, 33px items (19px line + 2×7 padding),
   and a *neutral* hover tint: violet is the accent, and an accent-tinted hover
   on every menu row spends it on navigation. */
QMenu {
    background: @surface@; color: @text@; border: 1px solid @borderStrong@;
    border-radius: @rControl@px; padding: 5px;
}
QMenu::item {
    padding: 7px 24px 7px 34px; min-height: 19px;
    border-radius: @rChrome@px; font-size: 13px;
}
QMenu::icon { left: 12px; }
QMenu::item:selected { background: @hoverTint@; color: @text@; }
QMenu::item:disabled { color: @disabled@; background: transparent; }
QMenu::separator { height: 1px; background: @border@; margin: 5px 8px; }

/* Semantic text roles — recoloured here so labels follow the theme toggle. */
QLabel[role="text"]       { color: @r_text@; }
QLabel[role="body"]       { color: @r_body@; }
QLabel[role="muted"]      { color: @r_muted@; }
QLabel[role="dim"]        { color: @r_dim@; }
QLabel[role="faint"]      { color: @r_faint@; }
QLabel[role="fainter"]    { color: @r_fainter@; }
QLabel[role="accent"]     { color: @r_accent@; }
QLabel[role="accentText"] { color: @r_accentText@; }
QLabel[role="accentIcon"] { color: @r_accentIcon@; }
QLabel[role="onAccent"]   { color: @r_onAccent@; }
QLabel[role="signal"]     { color: @r_signal@; }
QLabel[role="danger"]     { color: @r_danger@; }
QLabel[role="warning"]    { color: @r_warning@; }
)";

    // The semantic roles now resolve to the design system's text tiers rather
    // than to mirrored ramp steps, so every ported ui::label() lands on a
    // design-correct colour without touching its call site.
    s.replace("@r_text@", text().name());
    s.replace("@r_body@", textBody().name());
    s.replace("@r_muted@", textMuted().name());
    s.replace("@r_dim@", textMuted().name());
    s.replace("@r_faint@", textFaint().name());
    s.replace("@r_fainter@", textQuiet().name());
    s.replace("@r_accent@", primary().name());
    s.replace("@r_accentText@", primaryText().name());
    s.replace("@r_accentIcon@", primary().name());
    s.replace("@r_onAccent@", onPrimary().name());
    s.replace("@r_signal@", signal().name());
    s.replace("@r_danger@", danger().name());
    s.replace("@r_warning@", warning().name());

    s.replace("@text@", text().name());
    s.replace("@muted@", textMuted().name());
    s.replace("@raised@", surfaceRaised().name());
    s.replace("@surface@", surface().name());
    s.replace("@hoverTint@", rgba(textTint(0.07)));
    s.replace("@sunken@", surfaceSunken().name());
    s.replace("@primary@", primary().name());
    s.replace("@primarySoft@", rgba(primarySoft()));
    s.replace("@borderStrong@", rgba(borderStrong()));
    s.replace("@border@", rgba(borderSubtle()));
    s.replace("@sel@", rgba(selectionBg()));
    s.replace("@disabled@", textQuiet().name());
    s.replace("@thumb@", rgba(withAlpha(text(), 0.18)));
    s.replace("@thumbHover@", rgba(withAlpha(text(), 0.30)));
    s.replace("@rControl@", QString::number(tokens::radius::kControl));
    s.replace("@rChrome@", QString::number(tokens::radius::kChrome));
    s.replace("@mono@", monoFamily());
    return s;
}

void Theme::apply() {
    // The colour tokens above are pure and usable anywhere; installing a palette
    // and a stylesheet needs a QApplication. The headless test builds only a
    // QCoreApplication so it can assert on the tokens, and `qApp` is a
    // static_cast, not a checked one — so verify before dereferencing rather
    // than letting setMode() corrupt memory in a test binary.
    auto *widgetApp = qobject_cast<QApplication *>(QCoreApplication::instance());
    if (!widgetApp) return;

    QFont base(bodyFamily());
    base.setPixelSize(qMax(1, qRound(13 * m_uiScale)));
    base.setWeight(QFont::Normal);
    widgetApp->setFont(base);

    QPalette pal;
    pal.setColor(QPalette::Window, bg());
    pal.setColor(QPalette::WindowText, text());
    pal.setColor(QPalette::Base, surfaceSunken());
    pal.setColor(QPalette::AlternateBase, surface());
    pal.setColor(QPalette::Text, text());
    pal.setColor(QPalette::ButtonText, text());
    pal.setColor(QPalette::Button, surface());
    pal.setColor(QPalette::ToolTipBase, surfaceRaised());
    pal.setColor(QPalette::ToolTipText, text());
    pal.setColor(QPalette::Highlight, selectionBg());
    pal.setColor(QPalette::HighlightedText, text());
    pal.setColor(QPalette::PlaceholderText, textQuiet());
    pal.setColor(QPalette::Disabled, QPalette::Text, withAlpha(text(), 0.45));
    widgetApp->setPalette(pal);

    widgetApp->setStyleSheet(styleSheet());
}
