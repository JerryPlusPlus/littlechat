#include "ui/windows/mainwindow.h"
#include <QDir>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest>

class MainWindowTests : public QObject
{
    Q_OBJECT
  private slots:
    void initTestCase()
    {
        QDir().mkpath("test-temp");
        qputenv("TMP", QDir::current().absoluteFilePath("test-temp").toUtf8());
        qputenv("TEMP", QDir::current().absoluteFilePath("test-temp").toUtf8());
    }
    void searchSendAndDrafts()
    {
        QTemporaryDir dir;
        MainWindow window(dir.filePath("state.json"));
        window.show();
        auto *search = window.findChild<QLineEdit *>("searchEdit");
        auto *list = window.findChild<QListWidget *>("conversationList");
        auto *editor = window.findChild<QPlainTextEdit *>("messageEdit");
        QVERIFY(search);
        QVERIFY(list);
        QVERIFY(editor);
        QCOMPARE(list->count(), 12);
        search->setText("Java");
        QCOMPARE(list->count(), 1);
        search->setText(QStringLiteral("不存在的联系人"));
        QCOMPARE(list->count(), 0);
        search->clear();
        window.openConversation("0");
        QCOMPARE(window.store()->find("0")->unread, 0);
        editor->setPlainText(QStringLiteral("保留草稿"));
        window.openConversation("1");
        window.openConversation("0");
        QCOMPARE(editor->toPlainText(), QStringLiteral("保留草稿"));
        const int before = window.store()->find("0")->messages.size();
        QTest::keyClick(editor, Qt::Key_Return, Qt::ShiftModifier);
        QCOMPARE(window.store()->find("0")->messages.size(), before);
        QTest::keyClick(editor, Qt::Key_Return);
        QCOMPARE(window.store()->find("0")->messages.size(), before + 1);
        QVERIFY(editor->toPlainText().isEmpty());
        window.openConversation("2");
        QTRY_COMPARE_WITH_TIMEOUT(window.store()->find("0")->messages.size(), before + 2, 2000);
        QCOMPARE(window.store()->find("0")->unread, 1);
        QCOMPARE(window.currentConversation(), QString("2"));
    }
    void compactNavigation()
    {
        QTemporaryDir dir;
        MainWindow window(dir.filePath("state.json"));
        window.resize(480, 680);
        window.show();
        QTest::qWait(30);
        auto *panel = window.findChild<QWidget *>("listPanel");
        auto *chat = window.findChild<QWidget *>("chatPage");
        auto *back = window.findChild<QToolButton *>("backButton");
        QVERIFY(panel->isVisible());
        window.openConversation("0");
        QVERIFY(panel->isHidden());
        QVERIFY(chat->isVisible());
        QVERIFY(back->isVisible());
        QTest::mouseClick(back, Qt::LeftButton);
        QVERIFY(panel->isVisible());
        QVERIFY(window.currentConversation().isEmpty());
        window.resize(1068, 864);
        QTest::qWait(20);
        QVERIFY(panel->isVisible());
        window.resize(360, 420);
        QTest::qWait(20);
        QCOMPARE(window.width(), 360);
        window.openConversation("1");
        QVERIFY(chat->isVisible());
        QCOMPARE(window.width(), 360);
    }
};
QTEST_MAIN(MainWindowTests)
#include "tst_mainwindow.moc"
