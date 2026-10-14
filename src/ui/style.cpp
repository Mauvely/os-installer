#include "ui/style.h"

#include "core/theme.h"
#include "core/tokens.h"

#include <QFontDatabase>

// The fonts live in a static library, where nothing would pull the resource's
// initialiser into the link on its own. Q_INIT_RESOURCE cannot sit in a namespace.
static void initResources() { Q_INIT_RESOURCE(resources); }

namespace style {

void loadFonts() {
    initResources();
    // Baloo 2 ships as two static instances: Qt 6 has no variable-axis API, so
    // a variable file would leave every title to synthetic bold (app-base).
    for (const char *path : {":/fonts/Baloo2-Regular.ttf", ":/fonts/Baloo2-ExtraBold.ttf",
                             ":/fonts/Inter-Variable.ttf", ":/fonts/JetBrainsMono-Regular.ttf"}) {
        if (QFontDatabase::addApplicationFont(QString::fromLatin1(path)) < 0)
            qWarning("failed to load %s", path);
    }
}

QFont display(int px) {
    QFont f(Theme::displayFamily());
    f.setPixelSize(px);
    f.setWeight(static_cast<QFont::Weight>(tokens::type::kDisplay));
    f.setLetterSpacing(QFont::PercentageSpacing, tokens::type::kDisplayTracking);
    return f;
}

QFont body(int px, int weight) {
    QFont f(Theme::bodyFamily());
    f.setPixelSize(px);
    f.setWeight(static_cast<QFont::Weight>(weight));
    return f;
}

QFont mono(int px) {
    QFont f(Theme::monoFamily());
    f.setPixelSize(px);
    f.setStyleHint(QFont::Monospace);
    return f;
}

QFont eyebrow() {
    QFont f = mono(tokens::type::kXs);
    f.setCapitalization(QFont::AllUppercase);
    f.setLetterSpacing(QFont::PercentageSpacing, tokens::type::kLabelTracking);
    return f;
}

QString sheet() {
    const Theme &t = Theme::instance();
    const auto rgba = [](const QColor &c) {
        return QStringLiteral("rgba(%1,%2,%3,%4)")
            .arg(c.red()).arg(c.green()).arg(c.blue())
            .arg(QString::number(c.alphaF(), 'f', 3));
    };

    QString s = R"(
QWidget#installer { background: @bg@; }

QPushButton {
    min-height: @hBtn@px; padding: 0 22px; border-radius: @rControl@px;
    font-family: "@body@"; font-size: 15px; font-weight: 600;
    color: @text@; background: transparent; border: 1px solid @borderStrong@;
}
QPushButton:hover { background: @hoverTint@; }
QPushButton:focus { border-color: @primary@; }
QPushButton[kind="primary"] { color: @onPrimary@; background: @primary@; border: 1px solid @primary@; }
QPushButton[kind="primary"]:hover { background: @primaryHover@; border-color: @primaryHover@; }
QPushButton[kind="danger"] { color: @onPrimary@; background: @danger@; border: 1px solid @danger@; }
QPushButton[kind="danger"]:hover { background: @dangerHover@; border-color: @dangerHover@; }
QPushButton:disabled, QPushButton[kind="primary"]:disabled, QPushButton[kind="danger"]:disabled {
    color: @quiet@; background: @sunken@; border: 1px solid @border@;
}

QLineEdit { min-height: @hField@px; padding: 0 14px; font-size: 15px; }
QLineEdit[mono="true"] { font-family: "@mono@"; }

QCheckBox { color: @text@; font-size: 15px; spacing: 12px; }
QCheckBox::indicator {
    width: 20px; height: 20px; border-radius: @rChrome@px;
    border: 1px solid @borderStrong@; background: @sunken@;
}
QCheckBox::indicator:checked { background: @primary@; border-color: @primary@; }
QCheckBox::indicator:focus { border-color: @primary@; }
)";
    s.replace("@bg@", t.bg().name());
    s.replace("@text@", t.text().name());
    s.replace("@quiet@", t.textQuiet().name());
    s.replace("@sunken@", t.surfaceSunken().name());
    s.replace("@primary@", t.primary().name());
    s.replace("@primaryHover@", t.primaryHover().name());
    s.replace("@onPrimary@", t.onPrimary().name());
    s.replace("@danger@", t.danger().name());
    s.replace("@dangerHover@", t.danger().darker(112).name());
    s.replace("@hoverTint@", rgba(t.textTint(0.07)));
    s.replace("@borderStrong@", rgba(t.borderStrong()));
    s.replace("@border@", rgba(t.borderSubtle()));
    s.replace("@hBtn@", QString::number(tokens::control::kLg));
    s.replace("@hField@", QString::number(tokens::control::kLg - 4));
    s.replace("@rControl@", QString::number(tokens::radius::kControl));
    s.replace("@rChrome@", QString::number(tokens::radius::kChrome));
    s.replace("@body@", Theme::bodyFamily());
    s.replace("@mono@", Theme::monoFamily());
    return t.styleSheet() + s;
}

}  // namespace style
