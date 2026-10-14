#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  brand — the Mauvely brand palette (design reference v1.1), verbatim.
//
//  This header is the single source of truth for brand colour. Before it
//  existed the same hexes were re-typed in theme.cpp, document.h,
//  designpanel.cpp, coursepack.cpp, preview.cpp and style.css, and they drifted.
//  Everything Qt-side now reads from here; the two CSS files
//  (resources/export/style.css and the generated :root block) mirror these
//  values by hand and must be updated alongside.
//
//  The system: violet is the identity, mauve the supporting accent, teal a
//  locked signal accent used sparingly (progress fills, live/complete states),
//  slate the neutral text and greys. Danger and warning are invented hues tuned
//  to sit beside mauve and violet.
// ─────────────────────────────────────────────────────────────────────────────

#include <QColor>
#include <QString>

namespace brand {

// ── Ramps ────────────────────────────────────────────────────────────────────
// Eleven steps, 50 (lightest) … 950 (darkest). `Ramp::step()` accepts the
// hundreds notation used across the codebase and clamps to the nearest defined
// step, so callers can pass 100…900 without bounds-checking.
struct Ramp {
    const char *s50, *s100, *s200, *s300, *s400, *s500, *s600, *s700, *s800,
        *s900, *s950;

    QColor step(int s) const {
        switch (s) {
        case 50:  return QColor(QLatin1String(s50));
        case 100: return QColor(QLatin1String(s100));
        case 200: return QColor(QLatin1String(s200));
        case 300: return QColor(QLatin1String(s300));
        case 400: return QColor(QLatin1String(s400));
        case 500: return QColor(QLatin1String(s500));
        case 600: return QColor(QLatin1String(s600));
        case 700: return QColor(QLatin1String(s700));
        case 800: return QColor(QLatin1String(s800));
        case 900: return QColor(QLatin1String(s900));
        default:  return QColor(QLatin1String(s >= 950 ? s950 : s900));
        }
    }
};

// Violet — brand identity.
inline const Ramp &violet() {
    static const Ramp r{"#f9f9ff", "#f0eefe", "#e7e5fe", "#d2cefd", "#b5abfc",
                        "#968ae0", "#796cbf", "#5d5294", "#423a6a", "#2b2741",
                        "#1a1728"};
    return r;
}

// Mauve — supporting accent (the wordmark's namesake).
inline const Ramp &mauve() {
    static const Ramp r{"#faf6fb", "#f2eaf4", "#e2d2e6", "#cbb0d1", "#ac89b6",
                        "#886193", "#6f4e79", "#5b4063", "#4a3651", "#3d2d43",
                        "#281c2d"};
    return r;
}

// Indigo — reserved for editorial/marketing surfaces; not yet used in-app.
inline const Ramp &indigo() {
    static const Ramp r{"#f2f3fb", "#e2e4f5", "#c5c9ea", "#9ea4d6", "#6f78bd",
                        "#4c5397", "#353b80", "#2b2f68", "#262a60", "#1c1f47",
                        "#12142e"};
    return r;
}

// Teal — the locked signal accent. Progress fills, live/complete states.
inline const Ramp &teal() {
    static const Ramp r{"#e6fbf9", "#c1f5f0", "#8aeae2", "#49d9cf", "#17c6bb",
                        "#0fb3a8", "#0d938b", "#0f736d", "#115a56", "#123f3c",
                        "#082523"};
    return r;
}

// Slate — neutral text and greys.
inline const Ramp &slate() {
    static const Ramp r{"#f8f9fd", "#f3f5fe", "#e4e7f5", "#cfd3e5", "#b2b6ca",
                        "#9397ab", "#75798c", "#595d6c", "#3f424d", "#292b31",
                        "#161826"};
    return r;
}

// ── Semantic roles ───────────────────────────────────────────────────────────
inline QColor identity()    { return violet().step(500); }  // wordmarks, flat fields
inline QColor primary()     { return violet().step(600); }  // filled interactive surfaces
inline QColor primaryHover(){ return violet().step(700); }
inline QColor accent()      { return mauve().step(500); }   // supporting accent
inline QColor accentHover() { return mauve().step(600); }
inline QColor signal()      { return teal().step(500); }    // progress, live, complete
inline QColor signalHover() { return teal().step(600); }
inline QColor success()     { return teal().step(600); }

inline QColor danger()      { return QColor(QLatin1String("#d94f6f")); }
inline QColor dangerHover() { return QColor(QLatin1String("#b93c59")); }
inline QColor dangerSoft()  { return QColor(QLatin1String("#fdecf0")); }
inline QColor warning()     { return QColor(QLatin1String("#e8a33d")); }
inline QColor warningHover(){ return QColor(QLatin1String("#c9852a")); }
inline QColor warningSoft() { return QColor(QLatin1String("#fdf1de")); }

// Grounds. The app (Compose, the SCORM viewer) sits on slate 950; marketing
// surfaces use the lighter page ground.
inline QColor appGround()   { return slate().step(950); }   // #161826
inline QColor pageGround()  { return QColor(QLatin1String("#eceaf0")); }
inline QColor cardSurface() { return QColor(QLatin1String("#ffffff")); }

inline QColor textStrong()  { return slate().step(900); }   // headings
inline QColor textBody()    { return slate().step(800); }
inline QColor textMuted()   { return slate().step(600); }
inline QColor textFaint()   { return slate().step(500); }

// ── Radii ────────────────────────────────────────────────────────────────────
// Not a strict grid — the brand board uses these freely. Panels 22, tiles 16,
// controls 10–12, chips 6–8.
namespace radius {
constexpr int kXs = 6;
constexpr int kSm = 8;
constexpr int kMd = 10;
constexpr int kLg = 12;
constexpr int kXl = 16;
constexpr int k2Xl = 22;
constexpr int k3Xl = 28;
constexpr int kPill = 999;
}  // namespace radius

// ── Faces ────────────────────────────────────────────────────────────────────
// Family names as registered with Qt by ui::iconfont::init(). Baloo 2 is the
// display face (ExtraBold titles, Regular subtitles — the brand's signature
// move); Inter carries body and UI; JetBrains Mono carries labels and code.
inline QString displayFamily() { return QStringLiteral("Baloo 2"); }
inline QString bodyFamily()    { return QStringLiteral("Inter"); }
inline QString monoFamily()    { return QStringLiteral("JetBrains Mono"); }

}  // namespace brand
