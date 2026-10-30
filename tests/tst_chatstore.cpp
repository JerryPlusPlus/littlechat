#include "core/storage/chatstore.h"
#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

class ChatStoreTests : public QObject
{
    Q_OBJECT
  private slots:
    void initTestCase()
    {
        QDir().mkpath("test-temp");
        qputenv("TMP", QDir::current().absoluteFilePath("test-temp").toUtf8());
        qputenv("TEMP", QDir::current().absoluteFilePath("test-temp").toUtf8());
    }
    void persistenceAndUnicode()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("state.json");
        QString id;
        {
            ChatStore store(path);
            QCOMPARE(store.conversations().size(), 12);
            id = store.create(QStringLiteral("测试群 🌱"));
            QVERIFY(!id.isEmpty());
            QVERIFY(!store.send(id, "  \n "));
            QVERIFY(!store.send("missing", "hello"));
            QVERIFY(store.send(id, QStringLiteral("你好 Qt 6\n第二行 😊")));
            auto *c = store.find(id);
            c->draft = QStringLiteral("未发送的草稿");
            c->pinned = true;
            c->muted = true;
            QVERIFY(store.save());
        }
        ChatStore restored(path);
        auto *c = restored.find(id);
        QVERIFY(c);
        QCOMPARE(c->name, QStringLiteral("测试群 🌱"));
        QCOMPARE(c->messages.size(), 1);
        QCOMPARE(c->messages[0].text, QStringLiteral("你好 Qt 6\n第二行 😊"));
        QVERIFY(c->messages[0].timestamp.isValid());
        QCOMPARE(c->draft, QStringLiteral("未发送的草稿"));
        QVERIFY(c->pinned);
        QVERIFY(c->muted);
    }
};
QTEST_GUILESS_MAIN(ChatStoreTests)
#include "tst_chatstore.moc"
