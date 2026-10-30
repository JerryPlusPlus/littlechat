#pragma once
#include "core/models/conversation.h"
#include <QObject>
#include <QVector>

class ChatStore : public QObject
{
    Q_OBJECT
  public:
    explicit ChatStore(const QString &filePath = {}, QObject *parent = nullptr);
    QVector<Conversation> &conversations()
    {
        return m_conversations;
    }
    const QVector<Conversation> &conversations() const
    {
        return m_conversations;
    }
    Conversation *find(const QString &id);
    QString create(const QString &name);
    bool send(const QString &id, const QString &text, bool outgoing = true);
    bool save();
  signals:
    void changed(const QString &id);
    void storageError(const QString &error);

  private:
    void seed();
    void load();
    QString m_filePath;
    QVector<Conversation> m_conversations;
};
