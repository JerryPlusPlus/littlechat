#pragma once
#include <QObject>
#include <QString>

class ChatStore;

// Coordinates the existing local demo. Real ChatService/Network integration is a later milestone.
class ChatController : public QObject
{
  public:
    explicit ChatController(ChatStore &store, QObject *parent = nullptr);
    void setActiveConversation(const QString &id);
    bool sendMessage(const QString &id, const QString &text);

  private:
    ChatStore &m_store;
    QString m_activeConversation;
};
