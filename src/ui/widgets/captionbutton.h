#pragma once
#include <QToolButton>

class CaptionButton : public QToolButton
{
    Q_OBJECT
  public:
    explicit CaptionButton(const QString &icon, QWidget *parent = nullptr);
    void setNativeHover(bool hover);
    void setGlyph(const QString &glyph);

  protected:
    void paintEvent(QPaintEvent *) override;

  private:
    QString m_glyph;
    bool m_nativeHover = false;
};
