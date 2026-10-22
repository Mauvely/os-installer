// ─────────────────────────────────────────────────────────────────────────────
//  tst_signin — mauvely-signin's protocol and navigation policy, which are pure,
//  and the binary itself as mauvely-session runs it: what it does with a bad
//  first line, with its argv and environment, and at the end of its input.
// ─────────────────────────────────────────────────────────────────────────────

#include "navpolicy.h"
#include "protocol.h"

#include <QElapsedTimer>
#include <QProcess>
#include <QTcpServer>
#include <QtTest>

#include <unistd.h>

using navpolicy::Action;
using navpolicy::Kind;

namespace {

const QString kCode = QStringLiteral("ABCD-EFGH");

navpolicy::Context context(const QString &current = QStringLiteral("https://mauvely.com/link")) {
    return {QUrl(QStringLiteral("https://mauvely.com")), QUrl(current), kCode};
}

Action mainFrame(const char *url, Kind kind, const navpolicy::Context &c = context()) {
    return navpolicy::decide(QUrl(QString::fromLatin1(url)), kind, true, c);
}

/** Runs the binary with `input` on its standard input, closed after it unless
 *  `keepOpen`. */
struct Run {
    QProcess process;

    explicit Run(const QStringList &args = {}, const QProcessEnvironment &env =
                                                   QProcessEnvironment::systemEnvironment()) {
        process.setProcessEnvironment(env);
        process.setProcessChannelMode(QProcess::SeparateChannels);
        process.start(QStringLiteral(SIGNIN_BINARY), args);
    }
    bool started() { return process.waitForStarted(5000); }
    void write(const QByteArray &bytes) { process.write(bytes); }
    void close() { process.closeWriteChannel(); }
    bool exited(int ms) { return process.waitForFinished(ms); }
};

}  // namespace

class TestSignin : public QObject {
    Q_OBJECT

private slots:
    // ── the first line ──────────────────────────────────────────────────────

    void plainCodes() {
        QVERIFY(protocol::plainCode(QStringLiteral("ABCD-EFGH")));
        QVERIFY(protocol::plainCode(QStringLiteral("abcd2")));
        QVERIFY(protocol::plainCode(QString(32, QLatin1Char('A'))));
        for (const char *bad : {"", "ABCD EFGH", "ABCD\nEFGH", "<b>", "ABCD&x", "ABCD%41",
                                "ABCD/EFGH", "ABCD?x", "ÄBCD"})
            QVERIFY2(!protocol::plainCode(QString::fromUtf8(bad)), bad);
        QVERIFY(!protocol::plainCode(QString(33, QLatin1Char('A'))));
    }

    void accountsOrigins() {
        for (const char *good : {"https://mauvely.com", "https://mauvely.dev",
                                 "http://localhost:8787", "http://127.0.0.1:8787",
                                 "http://[::1]:8787", "http://10.0.2.2:8787"})
            QVERIFY2(protocol::acceptableOrigin(QUrl(QString::fromLatin1(good))), good);
        for (const char *bad : {"", "mauvely.com", "http://mauvely.com", "ftp://mauvely.com",
                                "https://mauvely.com/", "https://mauvely.com/link",
                                "https://mauvely.com?x=1", "https://mauvely.com#x",
                                "https://user@mauvely.com", "https://user:pw@mauvely.com",
                                "http://192.168.1.10:8787", "javascript:alert(1)",
                                "file:///etc/passwd", "data:text/html,x"})
            QVERIFY2(!protocol::acceptableOrigin(QUrl(QString::fromLatin1(bad))), bad);
    }

    void theFirstLineIsTheCodeAndTheOrigin() {
        QString why;
        const auto start = protocol::parseStart(
            "{\"code\":\"ABCD-EFGH\",\"origin\":\"https://mauvely.com\"}", &why);
        QVERIFY2(start, qPrintable(why));
        QCOMPARE(start->code, kCode);
        QCOMPARE(start->origin, QUrl(QStringLiteral("https://mauvely.com")));
        // A key it does not know is not an error.
        QVERIFY(protocol::parseStart(
            "{\"v\":1,\"code\":\"ABCD-EFGH\",\"origin\":\"http://127.0.0.1:8787\"}"));
    }

    void aBadFirstLineIsRefusedWithoutRepeatingIt() {
        const QByteArray bad[] = {
            "",
            "ABCD-EFGH",
            "[\"ABCD-EFGH\"]",
            "{\"origin\":\"https://mauvely.com\"}",
            "{\"code\":\"ABCD-EFGH\"}",
            "{\"code\":1234,\"origin\":\"https://mauvely.com\"}",
            "{\"code\":\"ABCD-EFGH<script>\",\"origin\":\"https://mauvely.com\"}",
            "{\"code\":\"ABCD-EFGH\",\"origin\":\"https://evil.example/ABCD-EFGH\"}",
            "{\"code\":\"ABCD-EFGH\",\"origin\":\"http://mauvely.com\"}",
            "{\"code\":\"ABCD-EFGH\",\"origin\":\"https://mauvely.com/?ABCD-EFGH\"}",
        };
        for (const QByteArray &line : bad) {
            QString why;
            QVERIFY2(!protocol::parseStart(line, &why), line.constData());
            QVERIFY(!why.isEmpty());
            QVERIFY2(!why.contains(QLatin1String("ABCD")), qPrintable(why));
        }
    }

    void thePageIsTheLinkPageWithTheCode() {
        QCOMPARE(protocol::pageUrl({kCode, QUrl(QStringLiteral("https://mauvely.com"))}),
                 QUrl(QStringLiteral("https://mauvely.com/link?code=ABCD-EFGH&embed=os")));
        QCOMPARE(protocol::pageUrl({kCode, QUrl(QStringLiteral("http://[::1]:8787"))}),
                 QUrl(QStringLiteral("http://[::1]:8787/link?code=ABCD-EFGH&embed=os")));
    }

    void oneLineIsReadAndNothingAfterIt() {
        int fds[2];
        QCOMPARE(::pipe(fds), 0);
        const QByteArray input = "{\"code\":\"X\"}\nfinish\n";
        QCOMPARE(::write(fds[1], input.constData(), input.size()), input.size());
        QByteArray line;
        QVERIFY(protocol::readLine(fds[0], &line));
        QCOMPARE(line, QByteArray("{\"code\":\"X\"}"));
        // What follows is left for the window to see.
        char rest[16] = {};
        QCOMPARE(::read(fds[0], rest, sizeof rest), 7);
        QCOMPARE(QByteArray(rest, 7), QByteArray("finish\n"));
        ::close(fds[0]);
        ::close(fds[1]);
    }

    void noLineIsNoStart() {
        int fds[2];
        // The end of input before a newline.
        QCOMPARE(::pipe(fds), 0);
        QCOMPARE(::write(fds[1], "{\"code\"", 7), 7);
        ::close(fds[1]);
        QByteArray line;
        QVERIFY(!protocol::readLine(fds[0], &line));
        ::close(fds[0]);
        // Longer than any line mauvely-session sends.
        QCOMPARE(::pipe(fds), 0);
        const QByteArray tooLong(protocol::kMaxLine + 1, 'a');
        QVERIFY(::write(fds[1], tooLong.constData(), tooLong.size()) == tooLong.size());
        QVERIFY(!protocol::readLine(fds[0], &line));
        ::close(fds[0]);
        ::close(fds[1]);
    }

    // ── where the window goes ───────────────────────────────────────────────

    void theAccountsOriginLoadsWhateverStartedIt() {
        for (Kind k : {Kind::Link, Kind::Typed, Kind::Form, Kind::BackForward, Kind::Reload,
                       Kind::Redirect, Kind::Other})
            QCOMPARE(mainFrame("https://mauvely.com/signin?next=%2Flink", k), Action::Load);
        QCOMPARE(mainFrame("https://mauvely.com:443/api/auth/sso/start?org=x", Kind::Link),
                 Action::Load);
    }

    void singleSignOnGoesToTheProviderAndComesBack() {
        // mauvely.com's SSO start redirects to the identity provider.
        QCOMPARE(mainFrame("https://login.microsoftonline.com/x/saml2", Kind::Redirect),
                 Action::Load);
        // The provider posts its answer back (SAML), or moves between its own
        // pages by script.
        const auto atIdp = context(QStringLiteral("https://login.microsoftonline.com/x/saml2"));
        QCOMPARE(mainFrame("https://mauvely.com/api/auth/sso/acs", Kind::Form, atIdp),
                 Action::Load);
        QCOMPARE(mainFrame("https://login.live.com/oauth", Kind::Other, atIdp), Action::Load);
        QCOMPARE(mainFrame("https://idp.example/next", Kind::Form, atIdp), Action::Load);
        // A link on the provider's page to its own origin stays in the window.
        QCOMPARE(mainFrame("https://login.microsoftonline.com/other", Kind::Link, atIdp),
                 Action::Load);
    }

    void aClickedLinkElsewhereGoesToTheBrowser() {
        QCOMPARE(mainFrame("https://docs.mauvely.com/privacy", Kind::Link), Action::External);
        QCOMPARE(mainFrame("https://example.com/", Kind::Link), Action::External);
        const auto atIdp = context(QStringLiteral("https://login.microsoftonline.com/x"));
        QCOMPARE(mainFrame("https://www.microsoft.com/privacy", Kind::Link, atIdp),
                 Action::External);
        QCOMPARE(mainFrame("mailto:help@mauvely.com", Kind::Link), Action::External);
        // Plain http to a real host: never loaded, but a click is the person's.
        QCOMPARE(mainFrame("http://example.com/", Kind::Link), Action::External);
    }

    void nothingElseLoads() {
        QCOMPARE(mainFrame("https://example.com/", Kind::Typed), Action::Refuse);
        QCOMPARE(mainFrame("http://example.com/", Kind::Redirect), Action::Refuse);
        QCOMPARE(mainFrame("http://idp.example/login", Kind::Form), Action::Refuse);
        for (const char *url : {"file:///etc/passwd", "data:text/html,<p>x", "view-source:https://mauvely.com",
                                "javascript:alert(1)", "chrome://settings", "qrc:/signin/mark.svg",
                                "blob:https://mauvely.com/1"}) {
            for (Kind k : {Kind::Link, Kind::Typed, Kind::Redirect, Kind::Other})
                QVERIFY2(mainFrame(url, k) == Action::Refuse, url);
        }
        QCOMPARE(mainFrame("about:blank", Kind::Other), Action::Load);
        QCOMPARE(mainFrame("about:config", Kind::Other), Action::Refuse);
    }

    void plainHttpIsForTheLocalStackOnly() {
        const navpolicy::Context local{QUrl(QStringLiteral("http://127.0.0.1:8787")),
                                       QUrl(QStringLiteral("http://127.0.0.1:8787/link")), kCode};
        QCOMPARE(navpolicy::decide(QUrl(QStringLiteral("http://127.0.0.1:8787/signin")),
                                   Kind::Link, true, local),
                 Action::Load);
        // A local identity provider beside it, for testing single sign-on.
        QCOMPARE(navpolicy::decide(QUrl(QStringLiteral("http://localhost:9000/idp")),
                                   Kind::Redirect, true, local),
                 Action::Load);
        QVERIFY(navpolicy::secureEnough(QUrl(QStringLiteral("http://[::1]:1/"))));
        QVERIFY(!navpolicy::secureEnough(QUrl(QStringLiteral("http://192.168.1.149/"))));
    }

    void framesLoadTheWebAndNothingElse() {
        const auto c = context();
        for (const char *url : {"https://challenges.cloudflare.com/x", "http://example.com/",
                                "about:blank", "data:text/html,x", "blob:https://mauvely.com/1"})
            QVERIFY2(navpolicy::decide(QUrl(QString::fromLatin1(url)), Kind::Other, false, c)
                         == Action::Load,
                     url);
        for (const char *url : {"file:///etc/passwd", "mailto:a@b.c", "view-source:https://x"})
            QVERIFY2(navpolicy::decide(QUrl(QString::fromLatin1(url)), Kind::Link, false, c)
                         == Action::Refuse,
                     url);
    }

    void newWindowsGoToTheBrowser() {
        const auto c = context();
        QCOMPARE(navpolicy::newWindow(QUrl(QStringLiteral("https://mauvely.com/terms")), c),
                 Action::External);
        QCOMPARE(navpolicy::newWindow(QUrl(QStringLiteral("https://example.com")), c),
                 Action::External);
        QCOMPARE(navpolicy::newWindow(QUrl(QStringLiteral("view-source:https://mauvely.com")), c),
                 Action::Refuse);
        QCOMPARE(navpolicy::newWindow(QUrl(QStringLiteral("file:///etc/passwd")), c),
                 Action::Refuse);
        QCOMPARE(navpolicy::newWindow(QUrl(), c), Action::Refuse);
    }

    void theCodeIsNeverHandedToTheBrowser() {
        const auto c = context();
        for (const char *url : {"https://mauvely.com/link?code=ABCD-EFGH",
                                "https://example.com/?c=abcd-efgh",
                                "https://example.com/ABCD%2DEFGH",
                                "https://example.com/#ABCD-EFGH",
                                "mailto:x@y.z?body=ABCD-EFGH"}) {
            const QUrl u(QString::fromLatin1(url));
            QVERIFY2(!navpolicy::openable(u, kCode), url);
            QVERIFY2(navpolicy::newWindow(u, c) == Action::Refuse, url);
            QVERIFY2(navpolicy::decide(u, Kind::Link, true,
                                       context(QStringLiteral("https://other.example/")))
                         != Action::External,
                     url);
        }
    }

    void theHeaderSaysWhereThePageIs() {
        QCOMPARE(navpolicy::hostLine(QUrl(QStringLiteral("https://mauvely.com/link?code=X"))),
                 QStringLiteral("mauvely.com"));
        QCOMPARE(navpolicy::hostLine(QUrl(QStringLiteral("https://login.microsoftonline.com:443/"))),
                 QStringLiteral("login.microsoftonline.com"));
        QCOMPARE(navpolicy::hostLine(QUrl(QStringLiteral("http://127.0.0.1:8787/link"))),
                 QStringLiteral("127.0.0.1:8787"));
        QCOMPARE(navpolicy::hostLine(QUrl(QStringLiteral("http://[::1]:8787/"))),
                 QStringLiteral("[::1]:8787"));
        QCOMPARE(navpolicy::hostLine(QUrl(QStringLiteral("about:blank"))), QString());
    }

    // ── the binary ──────────────────────────────────────────────────────────

    void aBadFirstLineExitsTwoAtOnce() {
        for (const QByteArray &input :
             {QByteArray("not json\n"), QByteArray("{\"code\":\"<b>\",\"origin\":\"https://mauvely.com\"}" "\n"),
              QByteArray()}) {
            Run run;
            QVERIFY(run.started());
            run.write(input);
            run.close();
            QVERIFY(run.exited(10000));
            QCOMPARE(run.process.exitStatus(), QProcess::NormalExit);
            QCOMPARE(run.process.exitCode(), 2);
            QVERIFY(!run.process.readAllStandardError().contains("<b>"));
        }
    }

    void theCommandLineIsNotAWayIn() {
        // A code and an origin on the command line are not read: with nothing
        // on standard input, there is nothing to show.
        Run run({QStringLiteral("--code=ABCD-EFGH"), QStringLiteral("https://mauvely.com")});
        QVERIFY(run.started());
        run.close();
        QVERIFY(run.exited(10000));
        QCOMPARE(run.process.exitCode(), 2);
    }

    void theEndOfInputFinishesItAndNoDebuggingPortOpens() {
        // A port the environment asks Chromium to listen on.
        QTcpServer probe;
        QVERIFY(probe.listen(QHostAddress::LocalHost, 0));
        const quint16 port = probe.serverPort();
        probe.close();

        // An accounts origin nothing answers on, so no page loads.
        QTcpServer closed;
        QVERIFY(closed.listen(QHostAddress::LocalHost, 0));
        const quint16 nobody = closed.serverPort();
        closed.close();

        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        env.insert(QStringLiteral("QTWEBENGINE_REMOTE_DEBUGGING"),
                   QStringLiteral("127.0.0.1:%1").arg(port));
        env.insert(QStringLiteral("QTWEBENGINE_CHROMIUM_FLAGS"),
                   QStringLiteral("--remote-debugging-port=%1").arg(port));
        Run run({}, env);
        QVERIFY(run.started());
        run.write(QStringLiteral("{\"code\":\"ABCD-EFGH\",\"origin\":\"http://127.0.0.1:%1\"}"
                                 "\n")
                      .arg(nobody)
                      .toUtf8());
        // Give WebEngine time to start, and a debugging server time to listen.
        QTest::qWait(4000);
        QCOMPARE(run.process.state(), QProcess::Running);
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, port);
        QVERIFY2(!socket.waitForConnected(1000), "something listens on the debugging port");

        run.write("finish\n");
        QElapsedTimer t;
        t.start();
        QVERIFY(run.exited(15000));
        QCOMPARE(run.process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(run.process.exitCode(), 0);
        // Bounded by the sign-out's three seconds.
        QVERIFY2(t.elapsed() < 6000, qPrintable(QString::number(t.elapsed())));
        QVERIFY(!run.process.readAllStandardError().contains("ABCD-EFGH"));
    }
};

QTEST_GUILESS_MAIN(TestSignin)
#include "tst_signin.moc"
