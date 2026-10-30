#pragma once
#include "message.h"
#include <QString>
#include <QVector>

struct Conversation
{
    QString id, name, preview, time, draft;
    int avatar = 0, unread = 0, members = 0;
    bool pinned = false, muted = false;
    QVector<Message> messages;
};
