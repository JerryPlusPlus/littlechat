#include "ui/windows/mainwindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QLibraryInfo>
#include <QTimer>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("littlechat");
    app.setOrganizationName("littlechat");
    app.setApplicationVersion("1.0.0");
    app.setStyle("Fusion");
    QFont font("Microsoft YaHei UI");
    font.setPixelSize(13);
    app.setFont(font);
    QTranslator translator;
    if (translator.load("qt_zh_CN", QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&translator);
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("littlechat · Qt 6 本地聊天演示"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"data-file", "Use an alternate conversation JSON file.", "path"});
    parser.addOption({"screenshot", "Save a UI screenshot and exit.", "path"});
    parser.addOption({"chat", "Open a conversation by id (0..11 for built-in chats).", "id"});
    parser.addOption({"compact", "Open at 480 x 680 for split-screen review."});
    parser.process(app);
    MainWindow window(parser.value("data-file"));
    if (parser.isSet("compact"))
        window.resize(480, 680);
    if (parser.isSet("chat"))
        window.openConversation(parser.value("chat"));
    window.show();
    if (parser.isSet("screenshot"))
        QTimer::singleShot(600, &window,
                           [&]
                           {
                               const bool saved = window.grab().save(parser.value("screenshot"));
                               app.exit(saved ? 0 : 1);
                           });
    return app.exec();
}
