#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  style — the installer's faces and its few rules on top of Theme's sheet.
//
//  app-base's `ui::font()`, `iconfont` and widget kit stay behind: the
//  installer is seven full-screen pages with buttons, two fields, a list and a
//  checkbox, and that is all this file styles. Colours come from Theme, sizes
//  from tokens, and nothing here types a hex.
// ─────────────────────────────────────────────────────────────────────────────

#include <QFont>
#include <QString>

namespace style {

/** Registers the embedded faces. The live system has no fonts of its own. */
void loadFonts();

QFont display(int px);                    // Baloo 2 ExtraBold, titles
QFont body(int px, int weight = 400);     // Inter
QFont mono(int px);                       // JetBrains Mono
QFont eyebrow();                          // mono, small, tracked

/** Theme's sheet plus buttons, fields and the checkbox. */
QString sheet();

}  // namespace style
