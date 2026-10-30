#pragma once
#include <QDateTime>
#include <QString>

struct Message
{
    QString text;
    QString sender;
    bool outgoing = false;
    QDateTime timestamp;
};
