#include "icons.h"
#include "resources.h"
#include <QFile>
#include <QPainter>
#include <QPainterPath>
#include <QSvgRenderer>

QIcon Icons::get(const QString &name, const QColor &color)
{
    initializeUiResources();
    QFile file(QStringLiteral(":/icons/%1.svg").arg(name));
    if (!file.open(QIODevice::ReadOnly))
    {
        file.setFileName(QStringLiteral(":/icons/chat.svg"));
        if (!file.open(QIODevice::ReadOnly))
            return {};
    }
    const QByteArray svg = file.readAll().replace("currentColor", color.name().toUtf8());
    QSvgRenderer renderer(svg);
    QIcon icon;
    for (int size : {24, 48, 72})
    {
        QPixmap pix(size, size);
        pix.fill(Qt::transparent);
        QPainter painter(&pix);
        renderer.render(&painter);
        painter.end();
        icon.addPixmap(pix);
    }
    return icon;
}

QPixmap Icons::avatar(int index, int size)
{
    QPixmap pix(size * 2, size * 2);
    pix.setDevicePixelRatio(2);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 48., size / 48.);
    const QList<QColor> colors{QColor("#254d41"), QColor("#fff9e5"), QColor("#edf8f3"), QColor("#f1f8ff"),
                               QColor("#e4f5d6"), QColor("#e8f6ff"), QColor("#ece7fa"), QColor("#fff3d0"),
                               QColor("#0785f9"), QColor("#0785f9"), QColor("#ff5c7b"), QColor("#ffa44d"),
                               QColor("#d9efff")};
    QPainterPath clip;
    clip.addEllipse(QRectF(0, 0, 48, 48));
    p.setClipPath(clip);
    p.fillRect(QRectF(0, 0, 48, 48), colors[index % colors.size()]);
    if (index == 0)
    {
        p.setPen(QPen(QColor("#bba76b"), 1));
        p.drawEllipse(QRectF(5, 5, 38, 38));
        p.setFont(QFont("Microsoft YaHei UI", 17, QFont::Bold));
        p.drawText(QRect(0, 4, 48, 36), Qt::AlignCenter, QStringLiteral("墨"));
        p.drawLine(10, 36, 38, 36);
    }
    else if (index >= 1 && index <= 3)
    {
        p.setPen(QPen(QColor("#269b86"), 2.5));
        p.drawRoundedRect(QRectF(12, 7, 24, 20), 5, 5);
        p.drawLine(18, 13, 15, 17);
        p.drawLine(15, 17, 18, 21);
        p.drawLine(30, 13, 33, 17);
        p.drawLine(33, 17, 30, 21);
        p.setPen(QColor("#268c7c"));
        p.setFont(QFont("Microsoft YaHei UI", 8, QFont::Bold));
        p.drawText(QRect(0, 28, 48, 16), Qt::AlignCenter, index == 1 ? "JAVA" : index == 2 ? "C++" : "WEB");
    }
    else if (index == 4)
    {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#20aa47"));
        p.drawRoundedRect(QRectF(10, 13, 28, 27), 8, 8);
        p.setBrush(Qt::white);
        p.drawEllipse(QRectF(16, 21, 6, 10));
        p.drawEllipse(QRectF(26, 21, 6, 10));
        p.setPen(QPen(QColor("#20aa47"), 3));
        p.drawLine(24, 6, 24, 13);
        p.drawLine(14, 7, 34, 7);
    }
    else if (index == 5)
    {
        p.save();
        p.translate(24, 24);
        p.rotate(45);
        p.setPen(QPen(QColor("#079de0"), 5));
        p.drawRoundedRect(QRectF(-15, -9, 23, 19), 3, 3);
        p.setPen(QPen(QColor("#20a770"), 5));
        p.drawRoundedRect(QRectF(-6, -9, 21, 19), 3, 3);
        p.restore();
    }
    else if (index == 6 || index == 12)
    {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#b1cbe1"));
        p.drawEllipse(QRectF(29, 7, 10, 10));
        QPainterPath hills;
        hills.moveTo(0, 48);
        hills.lineTo(0, 31);
        hills.cubicTo(15, 12, 20, 43, 36, 24);
        hills.lineTo(48, 32);
        hills.lineTo(48, 48);
        p.setBrush(QColor("#7288ad"));
        p.drawPath(hills);
        hills = QPainterPath();
        hills.moveTo(0, 48);
        hills.lineTo(0, 40);
        hills.cubicTo(19, 25, 30, 36, 48, 20);
        hills.lineTo(48, 48);
        p.setBrush(QColor("#415e79"));
        p.drawPath(hills);
    }
    else if (index == 7)
    {
        p.setPen(QPen(QColor("#aa8833"), 1.3));
        p.setBrush(QColor("#ffdc6d"));
        p.drawEllipse(QRectF(11, 14, 27, 27));
        p.drawEllipse(QRectF(13, 7, 22, 24));
        p.setBrush(QColor("#ffb554"));
        p.drawEllipse(QRectF(24, 19, 17, 7));
        p.setBrush(QColor("#50442b"));
        p.drawEllipse(QRectF(25, 13, 2.5, 3.5));
    }
    else
    {
        const QString icon = index == 8 ? "phone" : index == 9 ? "mail" : index == 10 ? "heart" : "bag";
        Icons::get(icon, Qt::white).paint(&p, QRect(12, 12, 24, 24));
    }
    return pix;
}

QPixmap Icons::brandMark(int size, const QColor &color)
{
    QPixmap pix(size * 2, size * 2);
    pix.setDevicePixelRatio(2);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 100., size / 100.);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawRoundedRect(QRectF(12, 18, 76, 56), 20, 20);
    QPainterPath tail;
    tail.moveTo(26, 66);
    tail.lineTo(23, 88);
    tail.lineTo(48, 69);
    p.drawPath(tail);
    p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    for (int x : {32, 50, 68})
        p.drawEllipse(QRectF(x - 4, 42, 8, 8));
    return pix;
}
