#pragma once
#include "core/controllers/chatcontroller.h"
#include "core/storage/chatstore.h"
#include "platform/window/framelesswindow.h"
class QLabel;
class QToolButton;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QScrollArea;
class QStackedWidget;
class QVBoxLayout;
class QHBoxLayout;
class QPushButton;

class MainWindow : public FramelessWindow
{
    Q_OBJECT
  public:
    explicit MainWindow(const QString &dataFile = {}, QWidget *parent = nullptr);
    ChatStore *store()
    {
        return &m_store;
    }
    QString currentConversation() const
    {
        return m_current;
    }
    void openConversation(const QString &id);

  protected:
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    void buildTitleBar(QVBoxLayout *root);
    void buildNavigation(QHBoxLayout *body);
    void buildConversationList(QHBoxLayout *body);
    void buildChat(QHBoxLayout *body);
    void refreshList();
    void renderMessages();
    void appendBubble(const Message &message, int avatar);
    void sendMessage();
    void newConversation();
    void attachFile();
    void showEmojis();
    void showChatMenu();
    void showAppMenu();
    void setSection(int section);
    void updateResponsiveLayout();
    void toast(const QString &text);
    void saveDraft();
    ChatStore m_store;
    ChatController m_controller;
    QString m_current;
    int m_section = 0;
    bool m_rendering = false;
    QWidget *m_navigation = nullptr, *m_listPanel = nullptr, *m_chatPage = nullptr, *m_emptyPage = nullptr,
            *m_messageHost = nullptr, *m_signature = nullptr, *m_weather = nullptr;
    QLineEdit *m_search = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_listEmpty = nullptr, *m_title = nullptr, *m_subtitle = nullptr, *m_notice = nullptr;
    QStackedWidget *m_pages = nullptr;
    QScrollArea *m_scroll = nullptr;
    QVBoxLayout *m_messages = nullptr;
    QPlainTextEdit *m_editor = nullptr;
    QToolButton *m_back = nullptr, *m_chatsButton = nullptr, *m_contactsButton = nullptr,
                *m_favoritesButton = nullptr;
    QPushButton *m_send = nullptr;
};
