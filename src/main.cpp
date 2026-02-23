#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QLockFile>
#include <QMessageBox>

#include "piano_assist/main_window.hpp"

#ifndef APP_VERSION
#define APP_VERSION "0.0.0"
#endif

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    QLockFile instance_lock(QDir::temp().absoluteFilePath("SheetMaster.lock"));
    instance_lock.setStaleLockTime(0);
    if (!instance_lock.tryLock(100)) {
        QMessageBox::information(nullptr, "SheetMaster", "SheetMaster is already running.");
        return 0;
    }

    QApplication::setApplicationName(QStringLiteral("SheetMaster"));
    QApplication::setApplicationDisplayName(QStringLiteral("SheetMaster"));
    QApplication::setOrganizationName(QStringLiteral("SheetMaster"));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));

    const QIcon app_icon(QStringLiteral(":/icons/app.png"));
    if (!app_icon.isNull()) {
        QApplication::setWindowIcon(app_icon);
    }

    piano_assist::MainWindow window;
    if (!app_icon.isNull()) {
        window.setWindowIcon(app_icon);
    }
    window.show();
    return QApplication::exec();
}
