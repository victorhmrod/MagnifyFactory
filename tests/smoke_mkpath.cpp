#include <QCoreApplication>
#include <QDebug>
#include <QDir>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    const QString path = QDir::homePath() + "/MagnifyFactory Output Test";
    QDir::root().rmpath(path); // best-effort cleanup from a previous run

    QDir outDir(path);
    qInfo() << "exists before:" << outDir.exists();

    const bool ok = QDir().mkpath(outDir.path());
    qInfo() << "mkpath(outDir.path()) returned:" << ok;
    qInfo() << "exists after:" << QDir(path).exists();
    return QDir(path).exists() ? 0 : 1;
}
