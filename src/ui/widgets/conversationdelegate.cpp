#include "conversationdelegate.h"
#include "ui/graphics/icons.h"
#include <QPainter>

using namespace ConversationRoles;

QSize ConversationDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const
{
    return QSize(260, 64);
}
void ConversationDelegate::paint(QPainter *p, const QStyleOptionViewItem &o, const QModelIndex &i) const
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setClipRect(o.rect);
    p->translate(o.rect.topLeft());
    const int w = o.rect.width();
    if (o.state & QStyle::State_Selected)
        p->fillRect(QRect(0, 0, w, 64), QColor("#e1edf6"));
    else if (o.state & QStyle::State_MouseOver)
        p->fillRect(QRect(0, 0, w, 64), QColor("#ededed"));
    else if (i.data(PinnedRole).toBool())
        p->fillRect(QRect(0, 0, w, 64), QColor("#eeeeee"));
    p->drawPixmap(QRect(12, 12, 40, 40), Icons::avatar(i.data(AvatarRole).toInt(), 40));
    QFont font = o.font;
    font.setPixelSize(13);
    p->setFont(font);
    p->setPen(QColor("#202327"));
    const QFontMetrics fm(font);
    p->drawText(QRect(64, 13, w - 130, 20), Qt::AlignVCenter,
                fm.elidedText(i.data(NameRole).toString(), Qt::ElideRight, w - 130));
    font.setPixelSize(10);
    p->setFont(font);
    p->setPen(QColor("#a1a4a8"));
    p->drawText(QRect(w - 66, 13, 54, 18), Qt::AlignRight | Qt::AlignVCenter, i.data(TimeRole).toString());
    font.setPixelSize(12);
    p->setFont(font);
    p->setPen(QColor("#999b9e"));
    const int unread = i.data(UnreadRole).toInt();
    const int reserve = unread ? 40 : 14;
    p->drawText(QRect(64, 35, w - 64 - reserve, 18), Qt::AlignVCenter,
                QFontMetrics(font).elidedText(i.data(PreviewRole).toString().replace('\n', ' '),
                                              Qt::ElideRight, w - 64 - reserve));
    if (unread)
    {
        const QString badge = unread > 99 ? "99+" : QString::number(unread);
        font.setPixelSize(10);
        font.setBold(true);
        p->setFont(font);
        const int bw = qMax(16, QFontMetrics(font).horizontalAdvance(badge) + 8);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor("#c5c7c9"));
        p->drawRoundedRect(QRectF(w - bw - 12, 37, bw, 16), 8, 8);
        p->setPen(Qt::white);
        p->drawText(QRect(w - bw - 12, 37, bw, 16), Qt::AlignCenter, badge);
    }
    else if (i.data(MutedRole).toBool())
        Icons::get("minimize", QColor("#b4b6b8")).paint(p, QRect(w - 29, 37, 16, 16));
    p->restore();
}
