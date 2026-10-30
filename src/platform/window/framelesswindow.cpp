#include "framelesswindow.h"
#include "ui/widgets/captionbutton.h"
#include <QAbstractButton>
#include <QEvent>
#include <QMouseEvent>
#include <QWindow>
#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>
#endif

FramelessWindow::FramelessWindow(QWidget *parent) : QWidget(parent)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowMinMaxButtonsHint |
                   Qt::WindowSystemMenuHint);
    // Keep an opaque HWND: per-pixel alpha or a window mask prevents DWM rounding.
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(360, 420);
}
void FramelessWindow::setTitleBar(QWidget *bar, CaptionButton *maximize)
{
    m_titleBar = bar;
    m_maximize = maximize;
    bar->installEventFilter(this);
}
void FramelessWindow::toggleMaximized()
{
    isMaximized() ? showNormal() : showMaximized();
}
void FramelessWindow::setupNativeFrame()
{
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    SetWindowLongPtrW(hwnd, GWL_STYLE,
                      style | WS_CAPTION | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU);
    // Numeric constants also compile against SDKs predating Windows 11.
    const DWORD round = 2;                                  // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd, 33, &round, sizeof(round)); // DWMWA_WINDOW_CORNER_PREFERENCE
    const COLORREF border = RGB(173, 207, 226);
    DwmSetWindowAttribute(hwnd, 34, &border, sizeof(border)); // DWMWA_BORDER_COLOR
    const MARGINS margins = {1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
#endif
    m_nativeReady = true;
}
void FramelessWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_nativeReady)
        setupNativeFrame();
}
void FramelessWindow::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && m_maximize)
    {
        m_maximize->setGlyph(isMaximized() ? "restore" : "maximize");
        m_maximize->setToolTip(isMaximized() ? QStringLiteral("还原")
                                             : QStringLiteral("最大化 · 悬停选择贴靠布局"));
        m_maximize->setNativeHover(false);
    }
}
bool FramelessWindow::eventFilter(QObject *watched, QEvent *event)
{
#ifndef Q_OS_WIN
    if (watched == m_titleBar)
    {
        if (event->type() == QEvent::MouseButtonPress &&
            static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton)
        {
            windowHandle()->startSystemMove();
            return true;
        }
        if (event->type() == QEvent::MouseButtonDblClick)
        {
            toggleMaximized();
            return true;
        }
    }
#else
    Q_UNUSED(watched)
    Q_UNUSED(event)
#endif
    return QWidget::eventFilter(watched, event);
}
bool FramelessWindow::nativeEvent(const QByteArray &type, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    MSG *msg = static_cast<MSG *>(message);
    const HWND hwnd = msg->hwnd;
    switch (msg->message)
    {
    case WM_NCCALCSIZE:
        if (msg->wParam)
        {
            auto *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);
            if (IsZoomed(hwnd))
            {
                MONITORINFO monitor = {};
                monitor.cbSize = sizeof(monitor);
                if (GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor))
                    params->rgrc[0] = monitor.rcWork;
            }
            *result = 0;
            return true;
        }
        break;
    case WM_GETMINMAXINFO:
    {
        auto *info = reinterpret_cast<MINMAXINFO *>(msg->lParam);
        MONITORINFO monitor = {};
        monitor.cbSize = sizeof(monitor);
        if (GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor))
        {
            info->ptMaxPosition = {monitor.rcWork.left - monitor.rcMonitor.left,
                                   monitor.rcWork.top - monitor.rcMonitor.top};
            info->ptMaxSize = {monitor.rcWork.right - monitor.rcWork.left,
                               monitor.rcWork.bottom - monitor.rcWork.top};
        }
        const qreal scale = devicePixelRatioF();
        info->ptMinTrackSize = {LONG(qRound(minimumWidth() * scale)), LONG(qRound(minimumHeight() * scale))};
        *result = 0;
        return true;
    }
    case WM_NCHITTEST:
    {
        const POINT screenPoint = {GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
        POINT clientPoint = screenPoint;
        ScreenToClient(hwnd, &clientPoint);
        const qreal scale = devicePixelRatioF();
        const QPoint pos(qRound(clientPoint.x / scale), qRound(clientPoint.y / scale));
        RECT rect;
        GetWindowRect(hwnd, &rect);
        const int border = qRound(6 * scale);
        if (!IsZoomed(hwnd))
        {
            const bool left = screenPoint.x < rect.left + border,
                       right = screenPoint.x >= rect.right - border;
            const bool top = screenPoint.y < rect.top + border,
                       bottom = screenPoint.y >= rect.bottom - border;
            if (top || bottom || left || right)
            {
                *result = top      ? (left    ? HTTOPLEFT
                                      : right ? HTTOPRIGHT
                                              : HTTOP)
                          : bottom ? (left    ? HTBOTTOMLEFT
                                      : right ? HTBOTTOMRIGHT
                                              : HTBOTTOM)
                          : left   ? HTLEFT
                                   : HTRIGHT;
                return true;
            }
        }
        if (m_maximize && QRect(m_maximize->mapTo(this, QPoint()), m_maximize->size()).contains(pos))
        {
            // DefWindowProc receives NC hover messages and opens the real Snap flyout.
            *result = HTMAXBUTTON;
            return true;
        }
        if (m_titleBar && QRect(m_titleBar->mapTo(this, QPoint()), m_titleBar->size()).contains(pos))
        {
            QWidget *child = childAt(pos);
            while (child && child != m_titleBar)
            {
                if (qobject_cast<QAbstractButton *>(child) || child->property("interactive").toBool())
                {
                    *result = HTCLIENT;
                    return true;
                }
                child = child->parentWidget();
            }
            *result = HTCAPTION;
            return true;
        }
        *result = HTCLIENT;
        return true;
    }
    case WM_NCMOUSEMOVE:
        if (m_maximize)
            m_maximize->setNativeHover(msg->wParam == HTMAXBUTTON);
        if (msg->wParam == HTMAXBUTTON)
        {
            TRACKMOUSEEVENT track = {sizeof(TRACKMOUSEEVENT), TME_LEAVE | TME_NONCLIENT, hwnd, 0};
            TrackMouseEvent(&track);
        }
        break;
    case WM_NCMOUSELEAVE:
        m_maxPressed = false;
        if (m_maximize)
        {
            m_maximize->setNativeHover(false);
            m_maximize->setDown(false);
        }
        break;
    case WM_NCLBUTTONDOWN:
        if (msg->wParam == HTMAXBUTTON && m_maximize)
        {
            m_maxPressed = true;
            m_maximize->setDown(true);
            *result = 0;
            return true;
        }
        break;
    case WM_NCLBUTTONUP:
        if (msg->wParam == HTMAXBUTTON && m_maximize)
        {
            const bool pressed = m_maxPressed;
            m_maxPressed = false;
            m_maximize->setDown(false);
            if (pressed)
                toggleMaximized();
            *result = 0;
            return true;
        }
        break;
    }
#endif
    return QWidget::nativeEvent(type, message, result);
}
