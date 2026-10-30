#include "messagebubble.h"
#include "ui/graphics/icons.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace
{
QLabel *label(const QString &text, const QString &name = {})
{
    auto *l = new QLabel(text);
    l->setTextFormat(Qt::PlainText);
    if (!name.isEmpty())
        l->setObjectName(name);
    return l;
}
} // namespace

MessageBubble::MessageBubble(const Message &message, int avatar, int availableWidth, QWidget *parent)
    : QWidget(parent)
{
    auto *row = this;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->setAlignment(Qt::AlignTop);
    auto *face = label({});
    face->setPixmap(Icons::avatar(message.outgoing ? 12 : avatar, 34));
    face->setFixedSize(34, 34);
    auto *content = new QWidget;
    auto *v = new QVBoxLayout(content);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(5);
    if (!message.outgoing)
    {
        auto *sender = label(message.sender);
        sender->setStyleSheet("color:#959ba3;font-size:11px;");
        v->addWidget(sender);
    }
    auto *bubble = label(message.text);
    bubble->setWordWrap(true);
    bubble->setTextInteractionFlags(Qt::TextSelectableByMouse);
    bubble->setCursor(Qt::IBeamCursor);
    bubble->setToolTip(message.timestamp.toString("yyyy-MM-dd HH:mm:ss"));
    bubble->setStyleSheet(message.outgoing
                              ? "background:#009dff;color:white;border-radius:9px;padding:11px 14px;"
                              : "background:white;color:#303740;border-radius:9px;padding:11px 14px;");
    bubble->ensurePolished();
    const int maxWidth = qMax(130, qMin(460, availableWidth - 132));
    int textWidth = 0;
    for (const auto &line : message.text.split('\n'))
        textWidth = qMax(textWidth, bubble->fontMetrics().horizontalAdvance(line));
    bubble->setFixedWidth(qBound(60, textWidth + 36, maxWidth));
    bubble->setFixedHeight(qMax(40, bubble->heightForWidth(bubble->width())));
    v->addWidget(bubble);
    content->setMaximumWidth(maxWidth);
    if (message.outgoing)
    {
        layout->addStretch();
        layout->addWidget(content);
        layout->addWidget(face);
    }
    else
    {
        layout->addWidget(face);
        layout->addWidget(content);
        layout->addStretch();
    }
}
