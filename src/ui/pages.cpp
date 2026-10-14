#include "ui/pages.h"

#include "core/enrolment.h"
#include "core/theme.h"
#include "core/tokens.h"
#include "ui/style.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QEnterEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStyle>
#include <QVariantAnimation>
#include <QVBoxLayout>

namespace ui {
namespace {

using namespace tokens;

constexpr int kColumn = 680;  // the recovery key at 26px mono, two lines, fits

QLabel *label(const QString &text, const QFont &font, const char *role, QWidget *parent = nullptr) {
    auto *l = new QLabel(text, parent);
    l->setFont(font);
    l->setProperty("role", role);
    l->setWordWrap(true);
    l->setTextFormat(Qt::PlainText);
    return l;
}

QLabel *fieldLabel(const QString &text) {
    return label(text, style::body(type::kSm, type::kMedium), "body");
}

QLabel *errorLabel() {
    auto *l = label(QString(), style::body(type::kSm), "danger");
    l->hide();
    return l;
}

// A sunken well. Padding goes in the sheet: a widget's own sheet replaces its
// contents margins.
QString wellSheet(int vertical, int horizontal) {
    const Theme &t = Theme::instance();
    return QStringLiteral("background: %1; border-radius: %2px; padding: %3px %4px;")
        .arg(t.surfaceSunken().name())
        .arg(radius::kLg)
        .arg(vertical)
        .arg(horizontal);
}

}  // namespace

// ── Page ─────────────────────────────────────────────────────────────────────

Page::Page(const QString &eyebrow, const QString &title, QWidget *parent) : QWidget(parent) {
    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(space::k48, 0, space::k48, space::k48);
    auto *column = new QWidget(this);
    column->setFixedWidth(kColumn);
    outer->addStretch(1);
    outer->addWidget(column);
    outer->addStretch(1);

    auto *v = new QVBoxLayout(column);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    v->addSpacing(space::k48 * 2);

    auto *eb = label(eyebrow, style::eyebrow(), "faint", column);
    v->addWidget(eb);
    v->addSpacing(space::k12);
    m_title = label(title, style::display(type::k3Xl), "text", column);
    v->addWidget(m_title);
    v->addSpacing(space::k16);
    m_body = label(QString(), style::body(type::kBase), "body", column);
    m_body->hide();
    v->addWidget(m_body);
    v->addSpacing(space::k32);

    m_content = new QVBoxLayout;
    m_content->setSpacing(space::k12);
    v->addLayout(m_content);
    v->addSpacing(space::k40);

    m_footer = new QWidget(column);
    auto *f = new QHBoxLayout(m_footer);
    f->setContentsMargins(0, 0, 0, 0);
    f->setSpacing(space::kInline);
    f->addStretch(1);
    v->addWidget(m_footer);
    v->addStretch(1);
}

void Page::setTitle(const QString &title) { m_title->setText(title); }

void Page::setBody(const QString &text) {
    m_body->setText(text);
    m_body->setVisible(!text.isEmpty());
}

QPushButton *Page::addButton(const QString &text, const char *kind, bool left) {
    auto *b = new QPushButton(text, m_footer);
    b->setProperty("kind", kind);
    b->setCursor(Qt::PointingHandCursor);
    auto *f = static_cast<QHBoxLayout *>(m_footer->layout());
    if (left)
        f->insertWidget(0, b);
    else
        f->addWidget(b);
    if (qstrcmp(kind, "secondary") != 0) b->setObjectName(QStringLiteral("primary"));
    return b;
}

// ── Welcome ──────────────────────────────────────────────────────────────────

WelcomePage::WelcomePage(bool hasTpm2, QWidget *parent)
    : Page(QStringLiteral("Mauvely OS"), QStringLiteral("Install Mauvely OS."), parent) {
    setObjectName(QStringLiteral("welcome"));
    setBody(QStringLiteral("You need an enrolment token from your organisation's admin."));
    if (!hasTpm2)
        content()->addWidget(label(QStringLiteral("This computer has no TPM 2.0. Mauvely OS needs one "
                                                  "to encrypt its disk."),
                                   style::body(type::kBase), "danger", this));
    auto *start = addButton(QStringLiteral("Start"), "primary");
    start->setEnabled(hasTpm2);
    connect(start, &QPushButton::clicked, this, &WelcomePage::next);
}

// ── Enrolment ────────────────────────────────────────────────────────────────

EnrolmentPage::EnrolmentPage(const QString &initialServer, QWidget *parent)
    : Page(QStringLiteral("Enrolment"), QStringLiteral("Enter your enrolment token."), parent) {
    setObjectName(QStringLiteral("enrolment"));

    content()->addWidget(fieldLabel(QStringLiteral("Server")));
    m_server = new QLineEdit(initialServer, this);
    m_server->setObjectName(QStringLiteral("server"));
    content()->addWidget(m_server);
    m_serverError = errorLabel();
    content()->addWidget(m_serverError);

    content()->addSpacing(space::k12);
    content()->addWidget(fieldLabel(QStringLiteral("Enrolment token")));
    m_token = new QLineEdit(this);
    m_token->setObjectName(QStringLiteral("token"));
    m_token->setProperty("mono", true);
    m_token->setPlaceholderText(QStringLiteral("mvd_…"));
    m_token->setMaxLength(256);
    content()->addWidget(m_token);
    m_tokenError = errorLabel();
    content()->addWidget(m_tokenError);

    auto *back = addButton(QStringLiteral("Back"), "secondary", true);
    auto *next = addButton(QStringLiteral("Continue"), "primary");
    connect(back, &QPushButton::clicked, this, &EnrolmentPage::back);
    connect(next, &QPushButton::clicked, this, &EnrolmentPage::submit);
    connect(m_token, &QLineEdit::returnPressed, this, &EnrolmentPage::submit);
    connect(m_server, &QLineEdit::returnPressed, m_token, qOverload<>(&QWidget::setFocus));
}

QString EnrolmentPage::server() const { return enrolment::normaliseServer(m_server->text()); }
QString EnrolmentPage::token() const { return enrolment::normaliseToken(m_token->text()); }
void EnrolmentPage::clearToken() { m_token->clear(); }

void EnrolmentPage::submit() {
    const bool serverOk = enrolment::isServerUrl(m_server->text());
    const bool tokenOk = enrolment::isTokenWellFormed(token());
    m_serverError->setText(QStringLiteral("Enter an address that starts with https://."));
    m_serverError->setVisible(!serverOk);
    m_tokenError->setText(QStringLiteral("Check the token. It starts with mvd_ and is %1 characters long.")
                              .arg(enrolment::kTokenLength));
    m_tokenError->setVisible(!tokenOk);
    if (serverOk && tokenOk) emit next();
}

// ── Disk ─────────────────────────────────────────────────────────────────────

DiskButton::DiskButton(const disks::Disk &disk, QWidget *parent)
    : QAbstractButton(parent), m_disk(disk) {
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setObjectName(QStringLiteral("disk:") + disk.path);
    setAccessibleName(disk.title() + QStringLiteral(", ") + disk.detail());
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QSize DiskButton::sizeHint() const { return {kColumn, 72}; }

void DiskButton::enterEvent(QEnterEvent *) { m_hover = true; update(); }
void DiskButton::leaveEvent(QEvent *) { m_hover = false; update(); }

void DiskButton::paintEvent(QPaintEvent *) {
    const Theme &t = Theme::instance();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const QColor fill = isChecked() ? t.primarySoft() : m_hover ? t.surfaceRaised() : t.surface();
    const QColor edge = isChecked() || hasFocus() ? t.primary() : t.borderSubtle();
    p.setPen(QPen(edge, 1));
    p.setBrush(fill);
    p.drawRoundedRect(r, radius::kCard, radius::kCard);

    // The radio dot.
    const QPointF c(space::k24 + 9, height() / 2.0);
    p.setPen(QPen(isChecked() ? t.primary() : t.textMuted(), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(c, 9, 9);
    if (isChecked()) {
        p.setPen(Qt::NoPen);
        p.setBrush(t.primary());
        p.drawEllipse(c, 4.5, 4.5);
    }

    const int x = space::k24 + 18 + space::k16;
    const QRect text(x, 0, width() - x - space::k24, height());
    p.setPen(t.text());
    p.setFont(style::body(type::kMd, type::kSemiBold));
    p.drawText(text.adjusted(0, 15, 0, 0), Qt::AlignLeft | Qt::AlignTop, m_disk.title());
    p.setPen(t.textFaint());
    p.setFont(style::mono(type::kXs));
    p.drawText(text.adjusted(0, 0, 0, -15), Qt::AlignLeft | Qt::AlignBottom, m_disk.detail());
}

DiskPage::DiskPage(QWidget *parent)
    : Page(QStringLiteral("Disk"), QStringLiteral("Choose a disk."), parent),
      m_group(new QButtonGroup(this)) {
    setObjectName(QStringLiteral("disk"));
    m_group->setExclusive(true);

    m_list = new QVBoxLayout;
    m_list->setSpacing(space::k10);
    content()->addLayout(m_list);
    m_empty = label(QStringLiteral("No disk to install to. Connect one, then refresh."),
                    style::body(type::kBase), "muted", this);
    m_empty->hide();
    content()->addWidget(m_empty);

    auto *back = addButton(QStringLiteral("Back"), "secondary", true);
    auto *refresh = addButton(QStringLiteral("Refresh"), "secondary");
    refresh->setObjectName(QStringLiteral("refresh"));
    m_next = addButton(QStringLiteral("Continue"), "primary");
    m_next->setEnabled(false);
    connect(back, &QPushButton::clicked, this, &DiskPage::back);
    connect(refresh, &QPushButton::clicked, this, &DiskPage::refresh);
    connect(m_next, &QPushButton::clicked, this, &DiskPage::next);
    connect(m_group, &QButtonGroup::buttonToggled, this, [this] { m_next->setEnabled(hasSelection()); });
}

void DiskPage::setDisks(const QList<disks::Disk> &eligible, const QString &error) {
    for (QAbstractButton *b : m_group->buttons()) {
        m_group->removeButton(b);
        b->deleteLater();
    }
    for (const disks::Disk &d : eligible) {
        auto *b = new DiskButton(d, this);
        m_group->addButton(b);
        m_list->addWidget(b);
    }
    m_empty->setText(error.isEmpty() ? QStringLiteral("No disk to install to. Connect one, then refresh.")
                                     : error);
    m_empty->setVisible(eligible.isEmpty());
    m_next->setEnabled(false);
}

bool DiskPage::hasSelection() const { return m_group->checkedButton() != nullptr; }

disks::Disk DiskPage::selected() const {
    auto *b = qobject_cast<DiskButton *>(m_group->checkedButton());
    return b ? b->disk() : disks::Disk{};
}

// ── Confirm ──────────────────────────────────────────────────────────────────

ConfirmPage::ConfirmPage(QWidget *parent)
    : Page(QStringLiteral("Erase disk"), QString(), parent) {
    setObjectName(QStringLiteral("confirm"));
    setBody(QStringLiteral("Everything on it is erased, and Mauvely OS is installed in its place. "
                           "This cannot be undone."));
    m_detail = label(QString(), style::mono(type::kSm), "muted", this);
    content()->addWidget(m_detail);

    auto *back = addButton(QStringLiteral("Back"), "secondary", true);
    m_erase = addButton(QStringLiteral("Erase and install"), "danger");
    connect(back, &QPushButton::clicked, this, &ConfirmPage::back);
    connect(m_erase, &QPushButton::clicked, this, [this] {
        m_erase->setEnabled(false);  // one click, one install
        emit confirmed();
    });
}

void ConfirmPage::setDisk(const disks::Disk &disk) {
    setTitle(QStringLiteral("Erase %1?").arg(disk.title()));
    m_detail->setText(disk.detail());
    m_erase->setEnabled(true);
}

// ── Install ──────────────────────────────────────────────────────────────────

BusyBar::BusyBar(QWidget *parent) : QWidget(parent), m_anim(new QVariantAnimation(this)) {
    setFixedHeight(6);
    m_anim->setStartValue(0.0);
    m_anim->setEndValue(1.0);
    m_anim->setDuration(1600);
    m_anim->setLoopCount(-1);
    connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_phase = v.toReal();
        update();
    });
    m_anim->start();
}

void BusyBar::paintEvent(QPaintEvent *) {
    const Theme &t = Theme::instance();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    const qreal r = height() / 2.0;
    p.setBrush(t.surfaceSunken());
    p.drawRoundedRect(rect(), r, r);

    // A third of the track, sweeping across and out.
    const qreal w = width() / 3.0;
    const qreal x = -w + m_phase * (width() + w);
    QPainterPath clip;
    clip.addRoundedRect(rect(), r, r);
    p.setClipPath(clip);
    p.setBrush(t.signal());
    p.drawRoundedRect(QRectF(x, 0, w, height()), r, r);
}

InstallPage::InstallPage(QWidget *parent)
    : Page(QStringLiteral("Installing"), QStringLiteral("Installing Mauvely OS."), parent) {
    setObjectName(QStringLiteral("install"));
    setBody(QStringLiteral("This takes a few minutes."));

    for (int i = 0; i < 3; ++i) {
        auto *row = new QHBoxLayout;
        row->setSpacing(space::k14);
        auto *dot = new QLabel(this);
        dot->setFixedSize(12, 12);
        auto *text = label(QString(), style::body(type::kBase), "muted", this);
        row->addWidget(dot, 0, Qt::AlignVCenter);
        row->addWidget(text, 1);
        content()->addLayout(row);
        m_dots << dot;
        m_labels << text;
    }
    content()->addSpacing(space::k24);
    content()->addWidget(new BusyBar(this));
    m_line = new QLabel(this);
    m_line->setFont(style::mono(type::kXs));
    m_line->setProperty("role", "faint");
    m_line->setTextFormat(Qt::PlainText);
    m_line->setMinimumWidth(1);  // elided below rather than widening the column
    content()->addWidget(m_line);
}

void InstallPage::reset(const disks::Disk &disk) {
    m_labels[0]->setText(QStringLiteral("Copy Mauvely OS to %1").arg(disk.title()));
    m_labels[1]->setText(QStringLiteral("Add a recovery key"));
    m_labels[2]->setText(QStringLiteral("Save the server and token"));
    m_line->clear();
    setStep(0);
}

void InstallPage::setStep(int step) {
    const Theme &t = Theme::instance();
    for (int i = 0; i < m_dots.size(); ++i) {
        QString dot;
        const char *role;
        if (i < step) {
            dot = QStringLiteral("background: %1; border-radius: 6px;").arg(t.primary().name());
            role = "body";
        } else if (i == step) {
            dot = QStringLiteral("border: 2px solid %1; border-radius: 6px;").arg(t.primary().name());
            role = "text";
        } else {
            dot = QStringLiteral("border: 2px solid %1; border-radius: 6px;").arg(t.textQuiet().name());
            role = "muted";
        }
        m_dots[i]->setStyleSheet(dot);
        m_labels[i]->setProperty("role", role);
        m_labels[i]->style()->unpolish(m_labels[i]);
        m_labels[i]->style()->polish(m_labels[i]);
    }
}

void InstallPage::setLine(const QString &line) {
    m_line->setText(m_line->fontMetrics().elidedText(line.trimmed(), Qt::ElideRight, kColumn));
}

// ── Recovery key ─────────────────────────────────────────────────────────────

RecoveryPage::RecoveryPage(QWidget *parent)
    : Page(QStringLiteral("Recovery key"), QStringLiteral("Write down the recovery key."), parent) {
    setObjectName(QStringLiteral("recovery"));
    setBody(QStringLiteral("It unlocks the disk if this computer ever fails to unlock it on its own."));

    m_key = new QLabel(this);
    m_key->setObjectName(QStringLiteral("key"));
    m_key->setFont(style::mono(26));
    m_key->setProperty("role", "text");
    m_key->setTextFormat(Qt::PlainText);
    m_key->setAlignment(Qt::AlignCenter);
    m_key->setStyleSheet(wellSheet(space::k24, space::k24));
    content()->addWidget(m_key);
    content()->addSpacing(space::k12);

    m_written = new QCheckBox(QStringLiteral("I have written down the recovery key"), this);
    m_written->setObjectName(QStringLiteral("written"));
    m_written->setCursor(Qt::PointingHandCursor);
    content()->addWidget(m_written);

    m_finish = addButton(QStringLiteral("Finish"), "primary");
    m_finish->setEnabled(false);
    connect(m_written, &QCheckBox::toggled, m_finish, &QPushButton::setEnabled);
    connect(m_finish, &QPushButton::clicked, this, &RecoveryPage::finished);
}

void RecoveryPage::setKey(const QString &key) {
    // Eight groups, four to a line. The dash that ends the first line is part
    // of the key, so the two lines read straight on into each other.
    const QStringList g = key.split(QLatin1Char('-'));
    m_key->setText(g.size() == 8 ? g.mid(0, 4).join(QLatin1Char('-')) + QStringLiteral("-\n") +
                                       g.mid(4).join(QLatin1Char('-'))
                                 : key);
    m_written->setChecked(false);
}

// ── Done ─────────────────────────────────────────────────────────────────────

DonePage::DonePage(QWidget *parent)
    : Page(QStringLiteral("Done"), QStringLiteral("Mauvely OS is installed."), parent) {
    setObjectName(QStringLiteral("done"));
    setBody(QStringLiteral("Remove the install medium, then restart."));
    auto *restart = addButton(QStringLiteral("Restart"), "primary");
    connect(restart, &QPushButton::clicked, this, &DonePage::restart);
}

// ── Failed ───────────────────────────────────────────────────────────────────

FailedPage::FailedPage(const QString &logPath, QWidget *parent)
    : Page(QStringLiteral("Install stopped"), QStringLiteral("The install did not finish."), parent) {
    setObjectName(QStringLiteral("failed"));
    m_tail = new QLabel(this);
    m_tail->setFont(style::mono(type::k2Xs));
    m_tail->setProperty("role", "muted");
    m_tail->setTextFormat(Qt::PlainText);
    m_tail->setWordWrap(true);
    m_tail->setStyleSheet(wellSheet(space::k14, space::k16));
    content()->addWidget(m_tail);
    content()->addWidget(label(logPath, style::mono(type::k2Xs), "faint", this));

    auto *again = addButton(QStringLiteral("Start again"), "primary");
    connect(again, &QPushButton::clicked, this, &FailedPage::again);
}

void FailedPage::setFailure(const QString &message, const QString &logTail) {
    setBody(message);
    const QStringList lines = logTail.split(QLatin1Char('\n'));
    m_tail->setText(lines.mid(qMax(0, lines.size() - 12)).join(QLatin1Char('\n')));
}

}  // namespace ui
