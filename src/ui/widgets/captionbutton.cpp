#include "captionbutton.h"
#include "ui/graphics/icons.h"
#include <QPainter>

CaptionButton::CaptionButton(const QString &icon, QWidget *parent) : QToolButton(parent), m_glyph(icon)
{
    setFixedSize(38, 40);
    setCursor(Qt::ArrowCursor);
    setFocusPolicy(Qt::NoFocus);
    setObjectName(icon + "Button");
    setAccessibleName(icon);
}
void CaptionButton::setNativeHover(bool hover)
{
    if (m_nativeHover != hover)
    {
        m_nativeHover = hover;
        update();
    }
}
void CaptionButton::setGlyph(const QString &glyph)
{
    m_glyph = glyph;
    update();
}
void CaptionButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    if (underMouse() || m_nativeHover || isDown())
        p.fillRect(rect(),
                   m_glyph == "close" ? QColor("#e54b55") : QColor(255, 255, 255, isDown() ? 65 : 35));
    Icons::get(m_glyph, Qt::white).paint(&p, QRect((width() - 16) / 2, (height() - 16) / 2, 16, 16));
}
