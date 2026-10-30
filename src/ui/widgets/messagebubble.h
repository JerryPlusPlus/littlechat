#pragma once
#include "core/models/message.h"
#include <QWidget>

class MessageBubble : public QWidget
{
  public:
    explicit MessageBubble(const Message &message, int avatar, int availableWidth, QWidget *parent = nullptr);
};
