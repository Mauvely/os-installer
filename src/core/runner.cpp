#include "core/runner.h"

#include <QProcess>
#include <QProcessEnvironment>

RunResult ProcessRunner::run(const QString &program, const QStringList &args,
                             const LineFn &onLine) {
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // A systemd tool whose stderr is the journal stream logs straight to the
    // journal. Removing JOURNAL_STREAM keeps every child's output on our pipe,
    // where the caller decides what is kept — systemd-cryptenroll's included.
    env.remove(QStringLiteral("JOURNAL_STREAM"));
    env.insert(QStringLiteral("SYSTEMD_LOG_TARGET"), QStringLiteral("console"));
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C.UTF-8"));  // the parsers read English
    p.setProcessEnvironment(env);

    p.start(program, args);
    if (!p.waitForStarted(-1)) return {-1, p.errorString()};
    p.closeWriteChannel();  // nothing may wait on a prompt

    RunResult r;
    QByteArray pending;
    const auto drain = [&](bool final) {
        pending += p.readAll();
        pending.replace('\r', '\n');
        int nl;
        while ((nl = pending.indexOf('\n')) >= 0) {
            const QString line = QString::fromUtf8(pending.left(nl));
            pending.remove(0, nl + 1);
            r.output += line + QLatin1Char('\n');
            if (onLine && !line.trimmed().isEmpty()) onLine(line);
        }
        if (final && !pending.isEmpty()) {
            const QString line = QString::fromUtf8(pending);
            pending.clear();
            r.output += line;
            if (onLine && !line.trimmed().isEmpty()) onLine(line);
        }
    };
    while (p.state() != QProcess::NotRunning) {
        p.waitForReadyRead(-1);
        drain(false);
    }
    p.waitForFinished(-1);
    drain(true);
    r.exitCode = p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1;
    return r;
}
