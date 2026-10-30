#include "ui/widgets/captionbutton.h"
#include "ui/windows/mainwindow.h"
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <dwmapi.h>
#include <windows.h>
#endif
#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

class WindowTests : public QObject
{
    Q_OBJECT
  private slots:
    void initTestCase()
    {
        QDir().mkpath("test-temp");
        qputenv("TMP", QDir::current().absoluteFilePath("test-temp").toUtf8());
        qputenv("TEMP", QDir::current().absoluteFilePath("test-temp").toUtf8());
    }
    void nativeSnapContract()
    {
#ifdef Q_OS_WIN
        if (QApplication::platformName() != "windows")
            QSKIP("Run with the Windows platform to verify HWND integration");
        QTemporaryDir dir;
        MainWindow window(dir.filePath("state.json"));
        window.resize(1068, 760);
        window.show();
        QTest::qWait(100);
        const HWND hwnd = reinterpret_cast<HWND>(window.winId());
        const auto style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        QVERIFY(style & WS_THICKFRAME);
        QVERIFY(style & WS_MAXIMIZEBOX);
        QVERIFY(style & WS_CAPTION);
        auto *maxi = window.findChild<CaptionButton *>("maximizeButton");
        QVERIFY(maxi);
        const QPoint local = maxi->mapTo(&window, maxi->rect().center());
        const qreal scale = window.devicePixelRatioF();
        POINT origin = {0, 0};
        ClientToScreen(hwnd, &origin);
        const LPARAM point =
            MAKELPARAM(origin.x + qRound(local.x() * scale), origin.y + qRound(local.y() * scale));
        QCOMPARE(SendMessageW(hwnd, WM_NCHITTEST, 0, point), LRESULT(HTMAXBUTTON));
        const LPARAM title = MAKELPARAM(origin.x + qRound(450 * scale), origin.y + qRound(22 * scale));
        QCOMPARE(SendMessageW(hwnd, WM_NCHITTEST, 0, title), LRESULT(HTCAPTION));
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTMAXBUTTON, point);
        SendMessageW(hwnd, WM_NCLBUTTONUP, HTMAXBUTTON, point);
        QTest::qWait(100);
        QVERIFY(window.isMaximized());
        MONITORINFO monitor = {};
        monitor.cbSize = sizeof(monitor);
        QVERIFY(GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor));
        RECT rect = {};
        GetWindowRect(hwnd, &rect);
        QVERIFY(rect.bottom <= monitor.rcWork.bottom);
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTMAXBUTTON, point);
        SendMessageW(hwnd, WM_NCLBUTTONUP, HTMAXBUTTON, point);
        QTest::qWait(100);
        QVERIFY(!window.isMaximized());
        DWORD round = 0;
        if (SUCCEEDED(DwmGetWindowAttribute(hwnd, 33, &round, sizeof(round))))
            QCOMPARE(round, DWORD(2));
#else
        QSKIP("Windows-only integration");
#endif
    }
};
QTEST_MAIN(WindowTests)
#include "tst_window_win.moc"
