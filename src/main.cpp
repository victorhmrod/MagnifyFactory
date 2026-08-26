#include <QApplication>
#include <QIcon>

#include "ui/MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("MagnifyFactory"));
    QApplication::setOrganizationName(QStringLiteral("MagnifyFactory"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icon.png")));

    magnify::ui::MainWindow window;
    window.show();

    // Launched from the Windows "Convert with MagnifyFactory" context menu:
    // argv[1] is the file the user right-clicked.
    const QStringList args = QApplication::arguments();
    if (args.size() > 1) {
        window.openExternalFile(args.at(1));
    }

    return QApplication::exec();
}
