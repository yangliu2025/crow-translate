#include "sourcetextedit.h"

#include <QSignalSpy>
#include <QTest>
#include <QToolButton>

class SourceTextEditTest : public QObject
{
    Q_OBJECT

private slots:
    void mergeLines_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");

        QTest::newRow("empty") << QString() << QString();
        QTest::newRow("single-line") << QStringLiteral("one  two\tthree") << QStringLiteral("one  two\tthree");
        QTest::newRow("single-breaks") << QStringLiteral("one\ntwo\nthree") << QStringLiteral("one two three");
        QTest::newRow("mixed-groups") << QStringLiteral("one\ntwo\n\nthree\n\n\nfour") << QStringLiteral("one two\nthree\n\nfour");
        QTest::newRow("whitespace-only-lines") << QStringLiteral("one\n \t\n\t\nthree") << QStringLiteral("one\n\nthree");
        QTest::newRow("unicode-whitespace") << QStringLiteral("one\n\u00a0\u2003\nthree") << QStringLiteral("one\nthree");
        QTest::newRow("multiple-whitespace-groups")
            << QStringLiteral("  one\n \t\n\u2003\n two \n \u2003\n \t\n three  ")
            << QStringLiteral("  one\n\n two \n\n three  ");
        QTest::newRow("existing-spaces") << QStringLiteral("one \ntwo\n\tthree") << QStringLiteral("one two\tthree");
        QTest::newRow("edges") << QStringLiteral("\none\n") << QStringLiteral("one");
        QTest::newRow("only-breaks") << QStringLiteral("\n\n\n") << QStringLiteral("\n\n");
        QTest::newRow("only-whitespace-lines") << QStringLiteral("\n \t\n \u2003\n") << QStringLiteral("\n\n");
        QTest::newRow("crlf") << QStringLiteral("one\r\ntwo\r\n\r\nthree") << QStringLiteral("one two\nthree");
        QTest::newRow("unicode-text") << QStringLiteral("\u4e00\n\u4e8c\n\n\u4e09") << QStringLiteral("\u4e00 \u4e8c\n\u4e09");
    }

    void mergeLines()
    {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        SourceTextEdit edit;
        edit.setPlainText(input);
        edit.mergeLines();
        QCOMPARE(edit.toPlainText(), expected);
    }

    void repeatedClicksAndUndo()
    {
        SourceTextEdit edit;
        const QString original = QStringLiteral("one\ntwo\n \t\n\u2003\nthree\n \t\n \u2003\nfour");
        edit.setPlainText(original);
        QToolButton button;
        connect(&button, &QToolButton::clicked, &edit, &SourceTextEdit::mergeLines);

        button.click();
        QCOMPARE(edit.toPlainText(), QStringLiteral("one two\n\nthree\n\nfour"));
        button.click();
        QCOMPARE(edit.toPlainText(), QStringLiteral("one two\nthree\nfour"));
        button.click();
        QCOMPARE(edit.toPlainText(), QStringLiteral("one two three four"));

        QSignalSpy changed(&edit, &SourceTextEdit::textChanged);
        button.click();
        QCOMPARE(changed.count(), 0);
        edit.undo();
        QCOMPARE(edit.toPlainText(), QStringLiteral("one two\nthree\nfour"));
        edit.undo();
        QCOMPARE(edit.toPlainText(), QStringLiteral("one two\n\nthree\n\nfour"));
        edit.undo();
        QCOMPARE(edit.toPlainText(), original);
        edit.redo();
        QCOMPARE(edit.toPlainText(), QStringLiteral("one two\n\nthree\n\nfour"));
    }

    void autoTranslation()
    {
        SourceTextEdit edit;
        edit.setPlainText(QStringLiteral("one\ntwo\n\nthree"));
        edit.setListenForEdits(true);
        QSignalSpy edited(&edit, &SourceTextEdit::textEdited);
        edit.mergeLines();
        QTRY_COMPARE(edited.count(), 1);
        QCOMPARE(edit.toSourceText(), QStringLiteral("one two\nthree"));
    }
};

QTEST_MAIN(SourceTextEditTest)
#include "sourcetextedit_test.moc"
