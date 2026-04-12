/*
 * Unit tests for ActionParser - verifies the escape sequence grammar
 * matches the original Linux tscrt button-action behavior.
 */
#include "ActionParser.h"

#include <QTest>

using tscrt::ActionChunk;
using tscrt::parseAction;

class TestActionParser : public QObject {
    Q_OBJECT

private slots:
    void plainText();
    void carriageReturn();
    void newline();
    void tab();
    void escape();
    void backslash();
    void pause();
    void mixedSequence();
    void emptyString();
    void unknownEscape();
    void trailingBackslash();
};

void TestActionParser::plainText()
{
    const auto chunks = parseAction(QStringLiteral("hello"));
    QCOMPARE(chunks.size(), 1);
    QCOMPARE(chunks.first().bytes, QByteArray("hello"));
    QCOMPARE(chunks.first().pauseMs, 0);
}

void TestActionParser::carriageReturn()
{
    const auto chunks = parseAction(QStringLiteral("\\r"));
    QCOMPARE(chunks.size(), 1);
    QCOMPARE(chunks.first().bytes, QByteArray("\r"));
}

void TestActionParser::newline()
{
    const auto chunks = parseAction(QStringLiteral("a\\nb"));
    QCOMPARE(chunks.size(), 1);
    QCOMPARE(chunks.first().bytes, QByteArray("a\nb"));
}

void TestActionParser::tab()
{
    QCOMPARE(parseAction(QStringLiteral("\\t")).first().bytes, QByteArray("\t"));
}

void TestActionParser::escape()
{
    QCOMPARE(parseAction(QStringLiteral("\\e[A")).first().bytes,
             QByteArray("\x1b[A"));
}

void TestActionParser::backslash()
{
    QCOMPARE(parseAction(QStringLiteral("\\\\")).first().bytes, QByteArray("\\"));
}

void TestActionParser::pause()
{
    const auto chunks = parseAction(QStringLiteral("ls\\r\\pdf -h\\r"));
    QCOMPARE(chunks.size(), 2);
    QCOMPARE(chunks.at(0).bytes, QByteArray("ls\r"));
    QCOMPARE(chunks.at(0).pauseMs, 1000);
    QCOMPARE(chunks.at(1).bytes, QByteArray("df -h\r"));
    QCOMPARE(chunks.at(1).pauseMs, 0);
}

void TestActionParser::mixedSequence()
{
    const auto chunks = parseAction(QStringLiteral("admin\\r\\ppassword\\r"));
    QCOMPARE(chunks.size(), 2);
    QCOMPARE(chunks.at(0).bytes, QByteArray("admin\r"));
    QCOMPARE(chunks.at(0).pauseMs, 1000);
    QCOMPARE(chunks.at(1).bytes, QByteArray("password\r"));
}

void TestActionParser::emptyString()
{
    QVERIFY(parseAction(QString()).isEmpty());
}

void TestActionParser::unknownEscape()
{
    // Unknown escapes are dropped entirely (strict whitelist).
    const auto chunks = parseAction(QStringLiteral("a\\xb"));
    QCOMPARE(chunks.size(), 1);
    QCOMPARE(chunks.first().bytes, QByteArray("ab"));
}

void TestActionParser::trailingBackslash()
{
    // A trailing backslash is dropped rather than leaked to the wire.
    const auto chunks = parseAction(QStringLiteral("foo\\"));
    QCOMPARE(chunks.size(), 1);
    QCOMPARE(chunks.first().bytes, QByteArray("foo"));
}

QTEST_APPLESS_MAIN(TestActionParser)
#include "test_action_parser.moc"
