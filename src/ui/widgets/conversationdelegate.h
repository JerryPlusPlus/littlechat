#pragma once
#include <QStyledItemDelegate>

namespace ConversationRoles
{
enum Role
{
    IdRole = Qt::UserRole,
    NameRole,
    PreviewRole,
    TimeRole,
    AvatarRole,
    UnreadRole,
    PinnedRole,
    MutedRole
};
}

class ConversationDelegate : public QStyledItemDelegate
{
  public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override;
    void paint(QPainter *, const QStyleOptionViewItem &, const QModelIndex &) const override;
};
