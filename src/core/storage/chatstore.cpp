#include "chatstore.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

ChatStore::ChatStore(const QString &filePath, QObject *parent) : QObject(parent)
{
    m_filePath = filePath.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                                          "/conversations.json"
                                    : filePath;
    seed();
    load();
}
void ChatStore::seed()
{
    const QStringList names{QStringLiteral("C++未来技术交流群"),
                            QStringLiteral("Java学习小组"),
                            QStringLiteral("嵌入式开发交流"),
                            QStringLiteral("Web前端学习小组"),
                            QStringLiteral("开源工具交流群"),
                            QStringLiteral("音视频流媒体开发交流群"),
                            QStringLiteral("单片机学习交流群（2）"),
                            QStringLiteral("编程鸭（星梦奇缘）"),
                            QStringLiteral("我的手机"),
                            QStringLiteral("邮箱提醒"),
                            QStringLiteral("C++ DXF/LXL视频免费发布2群"),
                            QStringLiteral("Python数据科学")};
    const QStringList previews{QStringLiteral("[有新文件] 左山人：这么早啊，我好像…"),
                               QStringLiteral("学习助手：欢迎一起交流技术…"),
                               QStringLiteral("学习助手：欢迎一起交流技术…"),
                               QStringLiteral("学习助手：欢迎一起交流技术…"),
                               QStringLiteral("管理员智能助手小莎撤回了一条成员消息"),
                               QStringLiteral("Spock加入了群聊"),
                               QStringLiteral("管理员独顾茫撤回了一条成员消息"),
                               QStringLiteral("程序员鼓励师：羡慕了"),
                               QStringLiteral("微信图片_20261014140201_101_239.jpg"),
                               QStringLiteral("Facebook：有动态更新等你查看：4 条新通知"),
                               QStringLiteral("vc工程师撤回了一条消息"),
                               QStringLiteral("Q群管家：@水果过敏性患者 您好，欢迎…")};
    const QStringList times{"21:28",
                            QStringLiteral("昨天15:25"),
                            QStringLiteral("昨天15:17"),
                            QStringLiteral("昨天15:10"),
                            QStringLiteral("星期日"),
                            QStringLiteral("星期六"),
                            QStringLiteral("星期六"),
                            "10/14",
                            "10/14",
                            "10/11",
                            "10/10",
                            "08/17"};
    const QList<int> unreads{108, 3, 8, 8, 38, 18, 102, 72, 0, 0, 0, 3};
    for (int i = 0; i < names.size(); ++i)
    {
        Conversation c;
        c.id = QString::number(i);
        c.name = names[i];
        c.preview = previews[i];
        c.time = times[i];
        c.avatar = i;
        c.unread = unreads[i];
        c.members = (i < 8 || i > 9) ? 156 + i * 23 : 0;
        c.muted = i == 10;
        const auto now = QDateTime::currentDateTime().addSecs(-600);
        if (i == 8)
            c.messages.append({QStringLiteral("文件、备忘和灵感，都可以发到这里。"),
                               QStringLiteral("我的手机"), false, now});
        else
        {
            c.messages.append(
                {QStringLiteral("欢迎来到%1 👋").arg(c.name), QStringLiteral("小谷"), false, now});
            c.messages.append({QStringLiteral("一起交流技术，记录每一个小小的进步。"),
                               QStringLiteral("林间晚风"), false, now.addSecs(60)});
            c.messages.append(
                {QStringLiteral("大家好，很高兴认识大家！"), QStringLiteral("我"), true, now.addSecs(120)});
            c.messages.append(
                {QStringLiteral("这是本地演示会话。试着发送一条消息吧，内容会保存在这台电脑上。"),
                 QStringLiteral("小谷 · 演示助手"), false, now.addSecs(180)});
        }
        m_conversations.append(c);
    }
}
Conversation *ChatStore::find(const QString &id)
{
    for (auto &c : m_conversations)
        if (c.id == id)
            return &c;
    return nullptr;
}
QString ChatStore::create(const QString &name)
{
    const QString clean = name.trimmed().left(60);
    if (clean.isEmpty())
        return {};
    Conversation c;
    c.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    c.name = clean;
    c.avatar = 12;
    c.time = QTime::currentTime().toString("HH:mm");
    c.preview = QStringLiteral("开始新的聊天");
    m_conversations.prepend(c);
    save();
    emit changed(c.id);
    return c.id;
}
bool ChatStore::send(const QString &id, const QString &text, bool outgoing)
{
    auto *c = find(id);
    const QString clean = text.trimmed();
    if (!c || clean.isEmpty() || clean.size() > 10000)
        return false;
    const auto now = QDateTime::currentDateTime();
    c->messages.append(
        {clean, outgoing ? QStringLiteral("我") : QStringLiteral("小谷 · 演示助手"), outgoing, now});
    c->preview = (outgoing ? QStringLiteral("我：") : QStringLiteral("演示助手：")) + clean;
    c->time = now.toString("HH:mm");
    if (outgoing)
        c->draft.clear();
    save();
    emit changed(id);
    return true;
}
bool ChatStore::save()
{
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());
    QJsonArray data;
    for (const auto &c : m_conversations)
    {
        QJsonArray messages;
        for (const auto &m : c.messages)
            messages.append(QJsonObject{{"text", m.text},
                                        {"sender", m.sender},
                                        {"outgoing", m.outgoing},
                                        {"timestamp", m.timestamp.toString(Qt::ISODateWithMs)}});
        data.append(QJsonObject{{"id", c.id},
                                {"name", c.name},
                                {"preview", c.preview},
                                {"time", c.time},
                                {"draft", c.draft},
                                {"avatar", c.avatar},
                                {"unread", c.unread},
                                {"members", c.members},
                                {"pinned", c.pinned},
                                {"muted", c.muted},
                                {"messages", messages}});
    }
    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(data).toJson()) < 0 || !file.commit())
    {
        emit storageError(file.errorString());
        return false;
    }
    return true;
}
void ChatStore::load()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        return;
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray() || doc.array().isEmpty())
        return;
    QVector<Conversation> loaded;
    for (const auto &value : doc.array())
    {
        const auto o = value.toObject();
        Conversation c;
        c.id = o["id"].toString();
        c.name = o["name"].toString();
        if (c.id.isEmpty() || c.name.isEmpty())
            continue;
        c.preview = o["preview"].toString();
        c.time = o["time"].toString();
        c.draft = o["draft"].toString();
        c.avatar = qBound(0, o["avatar"].toInt(), 12);
        c.unread = qMax(0, o["unread"].toInt());
        c.members = o["members"].toInt();
        c.pinned = o["pinned"].toBool();
        c.muted = o["muted"].toBool();
        for (const auto &mv : o["messages"].toArray())
        {
            const auto m = mv.toObject();
            c.messages.append({m["text"].toString(), m["sender"].toString(), m["outgoing"].toBool(),
                               QDateTime::fromString(m["timestamp"].toString(), Qt::ISODateWithMs)});
        }
        loaded.append(c);
    }
    if (!loaded.isEmpty())
        m_conversations = loaded;
}
