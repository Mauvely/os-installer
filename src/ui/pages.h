#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  pages — one full-screen page per step of the install.
//
//  Each page owns its own widgets and says what happened through signals;
//  none of them knows another exists. ui/window.cpp wires them, which is
//  app-base's "all widget-to-widget wiring in one place" rule moved out of
//  main.cpp so that tests/tst_flow.cpp can drive the same wiring with a fake
//  runner.
//
//  Copy follows the design system's CONTENT FUNDAMENTALS: a headline is a
//  short sentence with a full stop, buttons are verbs, and each thing is said
//  once.
// ─────────────────────────────────────────────────────────────────────────────

#include "core/disks.h"

#include <QAbstractButton>
#include <QList>
#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
class QVariantAnimation;

namespace ui {

/** The frame every page shares: eyebrow, title, body, content, footer. */
class Page : public QWidget {
    Q_OBJECT
public:
    Page(const QString &eyebrow, const QString &title, QWidget *parent = nullptr);

protected:
    void setTitle(const QString &title);
    /** A paragraph in the body colour, or empty to hide it. */
    void setBody(const QString &text);
    QVBoxLayout *content() const { return m_content; }
    /** Adds a button to the footer: `left` ones sit before the stretch. */
    QPushButton *addButton(const QString &text, const char *kind, bool left = false);

private:
    QLabel *m_title;
    QLabel *m_body;
    QVBoxLayout *m_content;
    QWidget *m_footer;
};

class WelcomePage : public Page {
    Q_OBJECT
public:
    explicit WelcomePage(bool hasTpm2, QWidget *parent = nullptr);
signals:
    void next();
};

class EnrolmentPage : public Page {
    Q_OBJECT
public:
    explicit EnrolmentPage(const QString &initialServer, QWidget *parent = nullptr);
    QString server() const;  // normalised
    QString token() const;   // normalised
    /** Empties the token field, so the secret does not sit in a widget. */
    void clearToken();
signals:
    void back();
    void next();

private:
    void submit();

    QLineEdit *m_server;
    QLineEdit *m_token;
    QLabel *m_serverError;
    QLabel *m_tokenError;
};

/** One disk in the list: a card with a radio dot, the model and a mono
 *  metadata line. Checkable; nothing is checked until someone clicks. */
class DiskButton : public QAbstractButton {
    Q_OBJECT
public:
    DiskButton(const disks::Disk &disk, QWidget *parent = nullptr);
    const disks::Disk &disk() const { return m_disk; }
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    disks::Disk m_disk;
    bool m_hover = false;
};

class DiskPage : public Page {
    Q_OBJECT
public:
    explicit DiskPage(QWidget *parent = nullptr);
    /** Replaces the list and clears the selection. */
    void setDisks(const QList<disks::Disk> &eligible, const QString &error);
    bool hasSelection() const;
    disks::Disk selected() const;
signals:
    void back();
    void refresh();
    void next();

private:
    QVBoxLayout *m_list;
    QButtonGroup *m_group;
    QLabel *m_empty;
    QPushButton *m_next;
};

class ConfirmPage : public Page {
    Q_OBJECT
public:
    explicit ConfirmPage(QWidget *parent = nullptr);
    void setDisk(const disks::Disk &disk);
signals:
    void back();
    void confirmed();

private:
    QLabel *m_detail;
    QPushButton *m_erase;
};

/** An indeterminate bar: bootc says what it is doing, not how far it is. */
class BusyBar : public QWidget {
    Q_OBJECT
public:
    explicit BusyBar(QWidget *parent = nullptr);
    QSize sizeHint() const override { return {400, 6}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QVariantAnimation *m_anim;
    qreal m_phase = 0;
};

class InstallPage : public Page {
    Q_OBJECT
public:
    explicit InstallPage(QWidget *parent = nullptr);
    void reset(const disks::Disk &disk);
    void setStep(int step);
    void setLine(const QString &line);

private:
    QList<QLabel *> m_dots;
    QList<QLabel *> m_labels;
    QLabel *m_line;
};

class RecoveryPage : public Page {
    Q_OBJECT
public:
    explicit RecoveryPage(QWidget *parent = nullptr);
    /** Shows the key; an empty key clears it from the page. */
    void setKey(const QString &key);
signals:
    void finished();

private:
    QLabel *m_key;
    QCheckBox *m_written;
    QPushButton *m_finish;
};

class DonePage : public Page {
    Q_OBJECT
public:
    explicit DonePage(QWidget *parent = nullptr);
signals:
    void restart();
};

class FailedPage : public Page {
    Q_OBJECT
public:
    explicit FailedPage(const QString &logPath, QWidget *parent = nullptr);
    void setFailure(const QString &message, const QString &logTail);
signals:
    void again();

private:
    QLabel *m_tail;
};

}  // namespace ui
