#pragma once
#include <QWidget>

class CaptionButton;

class FramelessWindow : public QWidget
{
    Q_OBJECT
  public:
    explicit FramelessWindow(QWidget *parent = nullptr);
    void setTitleBar(QWidget *bar, CaptionButton *maximize);
    void toggleMaximized();

  protected:
    bool nativeEvent(const QByteArray &type, void *message, qintptr *result) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;

  private:
    void setupNativeFrame();
    QWidget *m_titleBar = nullptr;
    CaptionButton *m_maximize = nullptr;
    bool m_nativeReady = false;
    bool m_maxPressed = false;
};
