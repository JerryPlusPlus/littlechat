#include "chatcontroller.h"
#include "core/storage/chatstore.h"
#include <QTimer>

ChatController::ChatController(ChatStore &store, QObject *parent) : QObject(parent), m_store(store) {}

void ChatController::setActiveConversation(const QString &id)
{
    m_activeConversation = id;
}

bool ChatController::sendMessage(const QString &id, const QString &text)
{
    if (!m_store.send(id, text))
        return false;
    if (id == "8" || id == "9")
        return true;

    // A context-bound callback is cancelled when this controller is destroyed.
    QTimer::singleShot(
        900, this,
        [this, id]
        {
            Conversation *conversation = m_store.find(id);
            if (!conversation)
                return;
            if (id != m_activeConversation)
                ++conversation->unread;
            m_store.send(id, QStringLiteral("收到啦 😊 这是一条本地模拟回复。你发送的消息已保存在当前电脑。"),
                         false);
        });
    return true;
}
