#include "mainwindow.h"
#include "ui/graphics/icons.h"
#include "ui/graphics/resources.h"
#include "ui/widgets/captionbutton.h"
#include "ui/widgets/conversationdelegate.h"
#include "ui/widgets/messagebubble.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace
{
using namespace ConversationRoles;
QToolButton *tool(const QString &icon, const QString &tip, QWidget *parent = nullptr)
{
    auto *b = new QToolButton(parent);
    b->setIcon(Icons::get(icon));
    b->setIconSize(QSize(21, 21));
    b->setFixedSize(34, 34);
    b->setToolTip(tip);
    b->setAccessibleName(tip);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}
QLabel *label(const QString &text, const QString &name = {})
{
    auto *l = new QLabel(text);
    l->setTextFormat(Qt::PlainText);
    if (!name.isEmpty())
        l->setObjectName(name);
    return l;
}

} // namespace

MainWindow::MainWindow(const QString &dataFile, QWidget *parent)
    : FramelessWindow(parent), m_store(dataFile, this), m_controller(m_store, this)
{
    setObjectName("mainWindow");
    setWindowTitle(QStringLiteral("littlechat · 轻聊"));
    setWindowIcon(QIcon(Icons::avatar(8)));
    initializeUiResources();
    QFile stylesheet(":/styles/light.qss");
    if (stylesheet.open(QIODevice::ReadOnly))
        setStyleSheet(QString::fromUtf8(stylesheet.readAll()));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    buildTitleBar(root);
    auto *body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    root->addLayout(body, 1);
    buildNavigation(body);
    buildConversationList(body);
    buildChat(body);
    connect(&m_store, &ChatStore::changed, this,
            [this](const QString &id)
            {
                refreshList();
                if (id == m_current)
                    renderMessages();
            });
    connect(&m_store, &ChatStore::storageError, this,
            [this](const QString &error) { toast(QStringLiteral("保存失败：") + error); });
    auto *searchKey = new QShortcut(QKeySequence("Ctrl+K"), this);
    connect(searchKey, &QShortcut::activated, this,
            [this]
            {
                if (width() < 760)
                {
                    saveDraft();
                    m_current.clear();
                    m_controller.setActiveConversation({});
                    updateResponsiveLayout();
                }
                m_search->setFocus();
                m_search->selectAll();
            });
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escape, &QShortcut::activated, this,
            [this]
            {
                if (!m_search->text().isEmpty())
                    m_search->clear();
                else
                {
                    saveDraft();
                    m_current.clear();
                    m_controller.setActiveConversation({});
                    m_pages->setCurrentWidget(m_emptyPage);
                    updateResponsiveLayout();
                    m_list->clearSelection();
                }
            });
    refreshList();
    const QSize available = screen()->availableGeometry().size() - QSize(80, 80);
    resize(QSize(1068, 864).boundedTo(available).expandedTo(minimumSize()));
}

void MainWindow::buildTitleBar(QVBoxLayout *root)
{
    auto *bar = new QWidget;
    bar->setObjectName("titleBar");
    bar->setFixedHeight(40);
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(10, 0, 0, 0);
    layout->setSpacing(6);
    auto *brand = label("LC");
    brand->setPixmap(Icons::brandMark(16, QColor("#bfe6ff")));
    brand->setFixedWidth(16);
    layout->addWidget(brand);
    auto *qq = label("LC");
    qq->setStyleSheet("color:#c2e7ff;font-size:12px;");
    layout->addWidget(qq);
    layout->addSpacing(16);
    auto *avatar = label({});
    avatar->setPixmap(Icons::avatar(12, 24));
    avatar->setFixedSize(24, 24);
    layout->addWidget(avatar);
    auto *name = label(QStringLiteral("好柿花生"));
    name->setStyleSheet("font-weight:600;");
    layout->addWidget(name);
    m_signature = label(QStringLiteral("今天也要好好学习"));
    layout->addWidget(m_signature);
    layout->addStretch();
    m_weather = new QWidget;
    auto *weatherLayout = new QHBoxLayout(m_weather);
    weatherLayout->setContentsMargins(0, 0, 10, 0);
    weatherLayout->setSpacing(5);
    auto *cloud = label({});
    cloud->setPixmap(Icons::get("cloud", Qt::white).pixmap(18, 18));
    weatherLayout->addWidget(cloud);
    weatherLayout->addWidget(label(QStringLiteral("雨")));
    layout->addWidget(m_weather);
    auto *theme = tool("shirt", QStringLiteral("更换主题"));
    theme->setIcon(Icons::get("shirt", Qt::white));
    layout->addWidget(theme);
    connect(theme, &QToolButton::clicked, this,
            [this, theme]
            {
                QMenu menu;
                for (const auto &pair : QList<QPair<QString, QString>>{{QStringLiteral("晴空蓝"), "#269ff0"},
                                                                       {QStringLiteral("薄荷绿"), "#28a99a"},
                                                                       {QStringLiteral("暮光紫"), "#8675cf"}})
                    menu.addAction(pair.first, this,
                                   [this, pair]
                                   {
                                       findChild<QWidget *>("titleBar")
                                           ->setStyleSheet("QWidget#titleBar { background:" + pair.second +
                                                           "; }");
                                   });
                menu.exec(theme->mapToGlobal(QPoint(0, 36)));
            });
    auto *mini = new CaptionButton("minimize"), *maxi = new CaptionButton("maximize"),
         *close = new CaptionButton("close");
    mini->setToolTip(QStringLiteral("最小化"));
    maxi->setToolTip(QStringLiteral("最大化 · 悬停选择贴靠布局"));
    close->setToolTip(QStringLiteral("关闭"));
    layout->addWidget(mini);
    layout->addWidget(maxi);
    layout->addWidget(close);
    layout->setSpacing(3);
    connect(mini, &QToolButton::clicked, this, &QWidget::showMinimized);
    connect(maxi, &QToolButton::clicked, this, &FramelessWindow::toggleMaximized);
    connect(close, &QToolButton::clicked, this, &QWidget::close);
    root->addWidget(bar);
    setTitleBar(bar, maxi);
}

void MainWindow::buildNavigation(QHBoxLayout *body)
{
    m_navigation = new QWidget;
    m_navigation->setObjectName("navigation");
    m_navigation->setFixedWidth(54);
    auto *nav = new QVBoxLayout(m_navigation);
    nav->setContentsMargins(9, 10, 10, 10);
    nav->setSpacing(7);
    m_chatsButton = tool("chat", QStringLiteral("消息"));
    m_contactsButton = tool("user", QStringLiteral("联系人"));
    m_favoritesButton = tool("star", QStringLiteral("置顶会话"));
    const QList<QToolButton *> sections{m_chatsButton, m_contactsButton, m_favoritesButton};
    for (int i = 0; i < sections.size(); ++i)
    {
        auto *b = sections[i];
        b->setCheckable(true);
        nav->addWidget(b);
        connect(b, &QToolButton::clicked, this, [this, i] { setSection(i); });
    }
    m_chatsButton->setChecked(true);
    m_chatsButton->setIcon(Icons::get("chat", QColor("#009bfa")));
    auto *bag = tool("bag", QStringLiteral("文件助手"));
    nav->addWidget(bag);
    connect(bag, &QToolButton::clicked, this, [this] { openConversation("8"); });
    auto *apps = tool("grid", QStringLiteral("应用"));
    nav->addWidget(apps);
    connect(apps, &QToolButton::clicked, this, &MainWindow::showAppMenu);
    nav->addStretch();
    auto *mail = tool("mail", QStringLiteral("邮箱提醒"));
    nav->addWidget(mail);
    connect(mail, &QToolButton::clicked, this, [this] { openConversation("9"); });
    auto *mobile = tool("phone", QStringLiteral("我的手机"));
    nav->addWidget(mobile);
    connect(mobile, &QToolButton::clicked, this, [this] { openConversation("8"); });
    auto *menu = tool("menu", QStringLiteral("设置与帮助"));
    nav->addWidget(menu);
    connect(menu, &QToolButton::clicked, this, &MainWindow::showAppMenu);
    for (auto *b : {apps, mail})
    {
        auto *dot = new QLabel(b);
        dot->setGeometry(25, 2, 7, 7);
        dot->setStyleSheet("background:#ff654f;border-radius:3px;");
        dot->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    body->addWidget(m_navigation);
}

void MainWindow::buildConversationList(QHBoxLayout *body)
{
    m_listPanel = new QWidget;
    m_listPanel->setObjectName("listPanel");
    m_listPanel->setFixedWidth(320);
    auto *layout = new QVBoxLayout(m_listPanel);
    layout->setContentsMargins(0, 0, 1, 0);
    layout->setSpacing(0);
    auto *searchRow = new QHBoxLayout;
    searchRow->setContentsMargins(12, 12, 12, 10);
    searchRow->setSpacing(8);
    m_search = new QLineEdit;
    m_search->setObjectName("searchEdit");
    m_search->setPlaceholderText(QStringLiteral("搜索"));
    m_search->setAccessibleName(QStringLiteral("搜索会话"));
    m_search->addAction(Icons::get("search", QColor("#979ca0")), QLineEdit::LeadingPosition);
    m_search->setClearButtonEnabled(true);
    m_search->setFixedHeight(28);
    searchRow->addWidget(m_search, 1);
    auto *add = tool("plus", QStringLiteral("发起聊天"));
    add->setObjectName("newChatButton");
    add->setFixedSize(28, 28);
    add->setStyleSheet("QToolButton{background:#eaeaea;} QToolButton:hover{background:#dceaf6;}");
    searchRow->addWidget(add);
    layout->addLayout(searchRow);
    m_list = new QListWidget;
    m_list->setObjectName("conversationList");
    m_list->setMouseTracking(true);
    m_list->setItemDelegate(new ConversationDelegate(m_list));
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    layout->addWidget(m_list, 1);
    m_listEmpty = label(QStringLiteral("没有找到相关会话"));
    m_listEmpty->setAlignment(Qt::AlignCenter);
    m_listEmpty->setStyleSheet("color:#a0a5ac;padding:40px 0;");
    layout->addWidget(m_listEmpty);
    m_listEmpty->hide();
    connect(m_search, &QLineEdit::textChanged, this, &MainWindow::refreshList);
    connect(add, &QToolButton::clicked, this, &MainWindow::newConversation);
    connect(m_list, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item) { openConversation(item->data(IdRole).toString()); });
    connect(m_list, &QListWidget::itemActivated, this,
            [this](QListWidgetItem *item) { openConversation(item->data(IdRole).toString()); });
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QListWidget::customContextMenuRequested, this,
            [this](const QPoint &pos)
            {
                auto *item = m_list->itemAt(pos);
                if (!item)
                    return;
                const QString id = item->data(IdRole).toString();
                auto *c = m_store.find(id);
                if (!c)
                    return;
                QMenu menu;
                menu.addAction(c->pinned ? QStringLiteral("取消置顶") : QStringLiteral("置顶会话"), this,
                               [this, id]
                               {
                                   if (auto *c = m_store.find(id))
                                       c->pinned = !c->pinned;
                                   m_store.save();
                                   refreshList();
                               });
                menu.addAction(QStringLiteral("标为未读"), this,
                               [this, id]
                               {
                                   if (auto *c = m_store.find(id))
                                       c->unread = 1;
                                   m_store.save();
                                   refreshList();
                               });
                menu.addAction(c->muted ? QStringLiteral("开启消息提醒") : QStringLiteral("消息免打扰"), this,
                               [this, id]
                               {
                                   if (auto *c = m_store.find(id))
                                       c->muted = !c->muted;
                                   m_store.save();
                                   refreshList();
                               });
                menu.exec(m_list->viewport()->mapToGlobal(pos));
            });
    body->addWidget(m_listPanel);
}

void MainWindow::buildChat(QHBoxLayout *body)
{
    m_pages = new QStackedWidget;
    m_pages->setMinimumWidth(0);
    body->addWidget(m_pages, 1);
    m_emptyPage = new QWidget;
    m_emptyPage->setObjectName("emptyPage");
    auto *emptyLayout = new QVBoxLayout(m_emptyPage);
    auto *penguin = label({});
    penguin->setPixmap(Icons::brandMark(96, QColor("#eaeaea")));
    penguin->setAlignment(Qt::AlignCenter);
    emptyLayout->addStretch();
    emptyLayout->addWidget(penguin);
    emptyLayout->addStretch();
    m_pages->addWidget(m_emptyPage);
    m_chatPage = new QWidget;
    m_chatPage->setObjectName("chatPage");
    auto *chat = new QVBoxLayout(m_chatPage);
    chat->setContentsMargins(0, 0, 0, 0);
    chat->setSpacing(0);
    auto *header = new QWidget;
    header->setObjectName("chatHeader");
    header->setFixedHeight(68);
    auto *h = new QHBoxLayout(header);
    h->setContentsMargins(18, 8, 18, 8);
    h->setSpacing(9);
    m_back = tool("back", QStringLiteral("返回会话列表"));
    m_back->setObjectName("backButton");
    h->addWidget(m_back);
    auto *titles = new QVBoxLayout;
    titles->setSpacing(3);
    m_title = label({}, "chatTitle");
    m_title->setMinimumWidth(0);
    m_title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_subtitle = label({}, "subTitle");
    titles->addWidget(m_title);
    titles->addWidget(m_subtitle);
    h->addLayout(titles, 1);
    auto *more = tool("more", QStringLiteral("聊天设置"));
    h->addWidget(more);
    connect(more, &QToolButton::clicked, this, &MainWindow::showChatMenu);
    connect(m_back, &QToolButton::clicked, this,
            [this]
            {
                saveDraft();
                m_current.clear();
                m_controller.setActiveConversation({});
                m_pages->setCurrentWidget(m_emptyPage);
                updateResponsiveLayout();
                m_list->clearSelection();
            });
    chat->addWidget(header);
    m_notice = label(QStringLiteral("本地演示 · 消息仅保存在这台电脑上"), "notice");
    m_notice->setAlignment(Qt::AlignCenter);
    chat->addWidget(m_notice);
    m_scroll = new QScrollArea;
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_messageHost = new QWidget;
    m_messageHost->setStyleSheet("background:transparent;");
    m_messages = new QVBoxLayout(m_messageHost);
    m_messages->setContentsMargins(22, 22, 22, 20);
    m_messages->setSpacing(20);
    m_messages->addStretch();
    m_scroll->setWidget(m_messageHost);
    chat->addWidget(m_scroll, 1);
    auto *composer = new QWidget;
    composer->setObjectName("composer");
    composer->setFixedHeight(184);
    auto *compose = new QVBoxLayout(composer);
    compose->setContentsMargins(16, 8, 16, 12);
    compose->setSpacing(3);
    auto *tools = new QHBoxLayout;
    tools->setSpacing(5);
    auto *emoji = tool("smile", QStringLiteral("表情")),
         *file = tool("folder", QStringLiteral("添加本地文件记录")),
         *image = tool("image", QStringLiteral("添加图片文件记录")),
         *history = tool("history", QStringLiteral("查看最早的消息"));
    tools->addWidget(emoji);
    tools->addWidget(file);
    tools->addWidget(image);
    tools->addStretch();
    tools->addWidget(history);
    compose->addLayout(tools);
    connect(emoji, &QToolButton::clicked, this, &MainWindow::showEmojis);
    connect(file, &QToolButton::clicked, this, &MainWindow::attachFile);
    connect(image, &QToolButton::clicked, this, &MainWindow::attachFile);
    connect(history, &QToolButton::clicked, this, [this] { m_scroll->verticalScrollBar()->setValue(0); });
    m_editor = new QPlainTextEdit;
    m_editor->setObjectName("messageEdit");
    m_editor->setPlaceholderText(QStringLiteral("说点什么吧…"));
    m_editor->setAccessibleName(QStringLiteral("消息输入框"));
    m_editor->installEventFilter(this);
    m_editor->setTabChangesFocus(true);
    compose->addWidget(m_editor, 1);
    auto *sendRow = new QHBoxLayout;
    auto *hint = label(QStringLiteral("Enter 发送 / Shift+Enter 换行"));
    hint->setStyleSheet("color:#afb3b8;font-size:10px;");
    sendRow->addWidget(hint);
    sendRow->addStretch();
    m_send = new QPushButton(QStringLiteral("发送"));
    m_send->setObjectName("sendButton");
    m_send->setCursor(Qt::PointingHandCursor);
    m_send->setEnabled(false);
    sendRow->addWidget(m_send);
    compose->addLayout(sendRow);
    chat->addWidget(composer);
    connect(m_send, &QPushButton::clicked, this, &MainWindow::sendMessage);
    connect(m_editor, &QPlainTextEdit::textChanged, this,
            [this]
            {
                m_send->setEnabled(!m_editor->toPlainText().trimmed().isEmpty());
                if (!m_rendering)
                    saveDraft();
            });
    m_pages->addWidget(m_chatPage);
    m_pages->setCurrentWidget(m_emptyPage);
}

void MainWindow::refreshList()
{
    if (!m_list)
        return;
    const int scroll = m_list->verticalScrollBar()->value();
    m_list->clear();
    QVector<const Conversation *> entries;
    const QString query = m_search->text().trimmed();
    for (const auto &c : m_store.conversations())
    {
        if (m_section == 2 && !c.pinned)
            continue;
        if (!query.isEmpty() && !c.name.contains(query, Qt::CaseInsensitive) &&
            !c.preview.contains(query, Qt::CaseInsensitive))
            continue;
        entries.append(&c);
    }
    std::stable_sort(entries.begin(), entries.end(),
                     [this](const auto *a, const auto *b)
                     {
                         if (m_section == 1)
                             return QString::localeAwareCompare(a->name, b->name) < 0;
                         return a->pinned && !b->pinned;
                     });
    for (const auto *c : entries)
    {
        auto *item = new QListWidgetItem(m_list);
        item->setData(Qt::AccessibleTextRole, c->name);
        item->setData(IdRole, c->id);
        item->setData(NameRole, c->name);
        item->setData(PreviewRole, c->draft.isEmpty() ? c->preview : QStringLiteral("[草稿] ") + c->draft);
        item->setData(TimeRole, c->time);
        item->setData(AvatarRole, c->avatar);
        item->setData(UnreadRole, c->unread);
        item->setData(PinnedRole, c->pinned);
        item->setData(MutedRole, c->muted);
        item->setToolTip(c->name);
        if (c->id == m_current)
        {
            m_list->setCurrentItem(item);
            item->setSelected(true);
        }
    }
    m_list->verticalScrollBar()->setValue(scroll);
    m_listEmpty->setVisible(entries.isEmpty());
    m_listEmpty->setText(m_section == 2 && query.isEmpty()
                             ? QStringLiteral("还没有置顶会话\n右键会话可以置顶")
                             : QStringLiteral("没有找到相关会话"));
}
void MainWindow::setSection(int section)
{
    m_section = section;
    const QList<QToolButton *> buttons{m_chatsButton, m_contactsButton, m_favoritesButton};
    const QStringList icons{"chat", "user", "star"};
    for (int i = 0; i < buttons.size(); ++i)
    {
        buttons[i]->setChecked(i == section);
        buttons[i]->setIcon(Icons::get(icons[i], QColor(i == section ? "#009bfa" : "#333a43")));
    }
    if (width() < 760)
    {
        saveDraft();
        m_current.clear();
        m_controller.setActiveConversation({});
        updateResponsiveLayout();
    }
    m_search->setPlaceholderText(section == 1   ? QStringLiteral("搜索联系人 / 群聊")
                                 : section == 2 ? QStringLiteral("搜索置顶会话")
                                                : QStringLiteral("搜索"));
    refreshList();
}
void MainWindow::saveDraft()
{
    if (auto *c = m_store.find(m_current))
    {
        c->draft = m_editor->toPlainText();
        m_store.save();
    }
}
void MainWindow::openConversation(const QString &id)
{
    if (!m_store.find(id))
        return;
    saveDraft();
    m_current = id;
    m_controller.setActiveConversation(id);
    auto *c = m_store.find(id);
    c->unread = 0;
    m_title->setText(c->name);
    m_title->setToolTip(c->name);
    m_subtitle->setText(c->members ? QStringLiteral("%1 位成员 · 一起聊点有趣的").arg(c->members)
                                   : QStringLiteral("随时记录，轻松分享"));
    m_rendering = true;
    m_editor->setPlainText(c->draft);
    m_rendering = false;
    m_pages->setCurrentWidget(m_chatPage);
    updateResponsiveLayout();
    renderMessages();
    refreshList();
    m_store.save();
    m_editor->setFocus();
}
void MainWindow::renderMessages()
{
    auto *c = m_store.find(m_current);
    if (!c)
        return;
    while (auto *item = m_messages->takeAt(0))
    {
        delete item->widget();
        delete item;
    }
    if (!c->messages.isEmpty())
    {
        auto *time = label(c->messages.first().timestamp.toString(QStringLiteral("MM月dd日 HH:mm")));
        time->setAlignment(Qt::AlignCenter);
        time->setStyleSheet("color:#a5a9af;font-size:11px;");
        m_messages->addWidget(time);
    }
    for (const auto &message : c->messages)
        appendBubble(message, c->avatar);
    m_messages->addStretch();
    QTimer::singleShot(
        0, this,
        [this] { m_scroll->verticalScrollBar()->setValue(m_scroll->verticalScrollBar()->maximum()); });
}
void MainWindow::appendBubble(const Message &message, int avatar)
{
    m_messages->addWidget(new MessageBubble(message, avatar, m_pages->width(), m_messageHost));
}
void MainWindow::sendMessage()
{
    const QString text = m_editor->toPlainText();
    const QString id = m_current;
    if (text.trimmed().size() > 10000)
    {
        toast(QStringLiteral("消息最多支持 10000 个字符"));
        return;
    }
    if (!m_controller.sendMessage(id, text))
        return;
    m_editor->clear();
    m_editor->setFocus();
}
bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_editor && event->type() == QEvent::KeyPress)
    {
        auto *key = static_cast<QKeyEvent *>(event);
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) &&
            !(key->modifiers() & Qt::ShiftModifier))
        {
            sendMessage();
            return true;
        }
    }
    return FramelessWindow::eventFilter(watched, event);
}
void MainWindow::newConversation()
{
    bool ok = false;
    const QString name =
        QInputDialog::getText(this, QStringLiteral("发起聊天"),
                              QStringLiteral("给新会话起个名字（本地演示）"), QLineEdit::Normal, {}, &ok);
    if (ok && !name.trimmed().isEmpty())
    {
        setSection(0);
        m_search->clear();
        openConversation(m_store.create(name));
    }
}
void MainWindow::attachFile()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择文件 · 仅添加本地记录"));
    if (path.isEmpty())
        return;
    const QFileInfo info(path);
    m_editor->insertPlainText(QStringLiteral("📎 %1（%2 KB，本地文件记录）")
                                  .arg(info.fileName())
                                  .arg(qMax<qint64>(1, (info.size() + 1023) / 1024)));
}
void MainWindow::showEmojis()
{
    QMenu menu;
    for (const QString &emoji :
         QStringList{QStringLiteral("😊"), QStringLiteral("👋"), QStringLiteral("👍"), QStringLiteral("🎉"),
                     QStringLiteral("❤️"), QStringLiteral("☕"), QStringLiteral("🌱")})
        menu.addAction(emoji, this,
                       [this, emoji]
                       {
                           m_editor->insertPlainText(emoji);
                           m_editor->setFocus();
                       });
    menu.exec(m_editor->mapToGlobal(QPoint(0, 0)));
}
void MainWindow::showChatMenu()
{
    auto *c = m_store.find(m_current);
    if (!c)
        return;
    const QString id = m_current;
    QMenu menu;
    auto *info = menu.addAction(c->members ? QStringLiteral("群聊 · %1 位成员").arg(c->members)
                                           : QStringLiteral("本地会话"));
    info->setEnabled(false);
    menu.addSeparator();
    menu.addAction(c->pinned ? QStringLiteral("取消置顶") : QStringLiteral("置顶会话"), this,
                   [this, id]
                   {
                       if (auto *c = m_store.find(id))
                           c->pinned = !c->pinned;
                       m_store.save();
                       refreshList();
                   });
    menu.addAction(c->muted ? QStringLiteral("开启消息提醒") : QStringLiteral("消息免打扰"), this,
                   [this, id]
                   {
                       if (auto *c = m_store.find(id))
                           c->muted = !c->muted;
                       m_store.save();
                       refreshList();
                   });
    menu.addAction(QStringLiteral("修改会话名称"), this,
                   [this, id]
                   {
                       auto *c = m_store.find(id);
                       if (!c)
                           return;
                       bool ok = false;
                       const auto name =
                           QInputDialog::getText(this, QStringLiteral("修改名称"), QStringLiteral("会话名称"),
                                                 QLineEdit::Normal, c->name, &ok);
                       if (ok && !name.trimmed().isEmpty())
                       {
                           c->name = name.trimmed().left(60);
                           m_store.save();
                           m_title->setText(c->name);
                           refreshList();
                       }
                   });
    menu.exec(m_chatPage->mapToGlobal(QPoint(m_chatPage->width() - 190, 60)));
}
void MainWindow::showAppMenu()
{
    QMenu menu;
    menu.addAction(QStringLiteral("发起新聊天"), this, &MainWindow::newConversation);
    menu.addAction(QStringLiteral("搜索会话    Ctrl+K"), this,
                   [this]
                   {
                       if (width() < 760)
                       {
                           saveDraft();
                           m_current.clear();
                           m_controller.setActiveConversation({});
                           updateResponsiveLayout();
                       }
                       m_search->setFocus();
                   });
    menu.addSeparator();
    menu.addAction(QStringLiteral("关于 littlechat"), this,
                   [this]
                   {
                       QDialog dialog(this);
                       dialog.setWindowTitle(QStringLiteral("关于 littlechat"));
                       auto *v = new QVBoxLayout(&dialog);
                       v->setContentsMargins(28, 25, 28, 20);
                       v->setSpacing(15);
                       auto *title = label(QStringLiteral("littlechat · 轻聊"));
                       title->setStyleSheet("font-size:22px;font-weight:600;color:#169bf3;");
                       v->addWidget(title);
                       auto *desc = label(QStringLiteral(
                           "让每一次交流，轻一点。\n\nQt 6 桌面聊天界面 · "
                           "v1.0\n本地演示，无需登录。消息保存在当前电脑。\n\nWindows 11：悬停最大化按钮或按 "
                           "Win+Z\nEnter 发送 · Shift+Enter 换行 · Ctrl+K 搜索"));
                       desc->setWordWrap(true);
                       v->addWidget(desc);
                       auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
                       connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
                       v->addWidget(buttons);
                       dialog.exec();
                   });
    menu.exec(m_navigation->mapToGlobal(QPoint(45, m_navigation->height() - 120)));
}
void MainWindow::toast(const QString &text)
{
    auto *l = label(text);
    l->setParent(this);
    l->setWordWrap(true);
    l->setStyleSheet("background:#364454;color:white;border-radius:7px;padding:12px 20px;");
    l->setMaximumWidth(qMin(500, width() - 30));
    l->adjustSize();
    l->move((width() - l->width()) / 2, height() - l->height() - 25);
    l->show();
    l->raise();
    QTimer::singleShot(3500, l, &QObject::deleteLater);
}
void MainWindow::updateResponsiveLayout()
{
    if (!m_listPanel || !m_pages)
        return;
    const bool compact = width() < 760;
    const bool chatting = !m_current.isEmpty();
    m_listPanel->setVisible(!compact || !chatting);
    m_pages->setVisible(!compact || chatting);
    m_listPanel->setFixedWidth(compact ? qMax(0, width() - 54) : 320);
    m_back->setVisible(compact);
    m_signature->setVisible(width() > 590);
    m_weather->setVisible(width() > 490);
    if (auto *name = findChild<QLabel *>("chatTitle"))
        name->setMaximumWidth(qMax(120, m_pages->width() - 100));
}
void MainWindow::resizeEvent(QResizeEvent *event)
{
    FramelessWindow::resizeEvent(event);
    updateResponsiveLayout();
    if (!m_current.isEmpty())
        renderMessages();
}
void MainWindow::closeEvent(QCloseEvent *event)
{
    saveDraft();
    m_store.save();
    FramelessWindow::closeEvent(event);
}
