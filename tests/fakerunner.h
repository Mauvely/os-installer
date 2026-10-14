#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  FakeRunner — a scripted Runner, so the tests start no process at all.
//
//  `script` maps a program to what it answers. Every call is recorded, which
//  is how a test proves a command was (or was not) run. The default script is
//  a clean install on the VM fixture; a test replaces the entries it is about.
// ─────────────────────────────────────────────────────────────────────────────

#include "core/runner.h"

#include <QFile>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QStringList>

#include <functional>

inline QString fixture(const QString &name) {
    QFile f(QStringLiteral(FIXTURES "/") + name);
    if (!f.open(QIODevice::ReadOnly)) qFatal("missing fixture %s", qPrintable(name));
    return QString::fromUtf8(f.readAll());
}

struct FakeRunner : Runner {
    struct Call {
        QString program;
        QStringList args;
        QString line() const { return program + QLatin1Char(' ') + args.join(QLatin1Char(' ')); }
    };
    using Answer = std::function<RunResult(const QStringList &args)>;

    QHash<QString, Answer> script;
    /** Runs before each answer, on the caller's thread: the flow test uses it
     *  to hold bootc mid-install and print a line of progress. */
    std::function<void(const QString &program, const LineFn &onLine)> before;

    FakeRunner() { reset(); }

    /** A clean install onto /dev/vda of lsblk-vm.json. */
    void reset() {
        script.clear();
        before = nullptr;
        {
            QMutexLocker lock(&m_mutex);
            m_calls.clear();
        }
        script[QStringLiteral("findmnt")] = [](const QStringList &) { return RunResult{0, QStringLiteral("/dev/sr0\n")}; };
        script[QStringLiteral("lsblk")] = [](const QStringList &args) {
            // The LUKS query names the disk; the disk listing does not.
            return args.contains(QStringLiteral("/dev/vda")) ? RunResult{0, fixture(QStringLiteral("lsblk-luks.json"))}
                                                            : RunResult{0, fixture(QStringLiteral("lsblk-vm.json"))};
        };
        script[QStringLiteral("podman")] = [](const QStringList &) {
            return RunResult{0, fixture(QStringLiteral("podman-images.json"))};
        };
        script[QStringLiteral("bootc")] = [](const QStringList &) {
            return RunResult{1, fixture(QStringLiteral("bootc-leaked-mount.log"))};
        };
        script[QStringLiteral("systemd-cryptenroll")] = [](const QStringList &) {
            return RunResult{0, fixture(QStringLiteral("cryptenroll-recovery.txt"))};
        };
    }

    QList<Call> calls() const {
        QMutexLocker lock(&m_mutex);
        return m_calls;
    }

    QStringList lines() const {
        QStringList out;
        for (const Call &c : calls()) out << c.line();
        return out;
    }

    RunResult run(const QString &program, const QStringList &args, const LineFn &onLine = {}) override {
        {
            QMutexLocker lock(&m_mutex);
            m_calls << Call{program, args};
        }
        if (before) before(program, onLine);
        const Answer answer = script.value(program);
        const RunResult r = answer ? answer(args) : RunResult{0, QString()};
        if (onLine)
            for (const QString &line : r.output.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) onLine(line);
        return r;
    }

private:
    mutable QMutex m_mutex;
    QList<Call> m_calls;
};
