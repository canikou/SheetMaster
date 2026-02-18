#include <QApplication>
#include <QIcon>

#include "piano_assist/main_window.hpp"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("SheetMaster"));
    QApplication::setApplicationDisplayName(QStringLiteral("SheetMaster"));
    QApplication::setOrganizationName(QStringLiteral("SheetMaster"));

    const QIcon app_icon(QStringLiteral(":/icons/app.png"));
    if (!app_icon.isNull())
    {
        app.setWindowIcon(app_icon);
    }

    piano_assist::MainWindow window;
    if (!app_icon.isNull())
    {
        window.setWindowIcon(app_icon);
    }
    window.show();
    return app.exec();
}
