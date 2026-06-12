#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QLockFile>
#include <QMessageBox>

#include "piano_assist/app_info.hpp"
#include "piano_assist/main_window.hpp"

#ifndef APP_VERSION
#define APP_VERSION "0.0.0"
#endif

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    const QString app_name = QString::fromLatin1(piano_assist::AppInfo::kAppName);
    QLockFile instance_lock(QDir::temp().absoluteFilePath(QString("%1.lock").arg(app_name)));
    instance_lock.setStaleLockTime(0);
    if (!instance_lock.tryLock(100)) {
        QMessageBox::information(nullptr, app_name,
                                 QString("%1 is already running.").arg(app_name));
        return 0;
    }

    QApplication::setApplicationName(app_name);
    QApplication::setApplicationDisplayName(app_name);
    QApplication::setOrganizationName(app_name);
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
