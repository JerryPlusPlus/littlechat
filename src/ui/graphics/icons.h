#pragma once
#include <QColor>
#include <QIcon>
#include <QPixmap>
namespace Icons
{
QIcon get(const QString &name, const QColor &color = QColor("#333a43"));
QPixmap avatar(int index, int size = 44);
QPixmap brandMark(int size, const QColor &color);
} // namespace Icons
