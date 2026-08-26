// Verifies the QDirIterator recursive/non-recursive scan behavior
// MainWindow::addInputFolder() relies on, against a real directory tree.
// Not part of ctest (needs a directory laid out by the caller); run manually.
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <cstdio>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        fprintf(stderr, "Usage: smoke_folder_scan <dir>\n");
        return 1;
    }
    const QString dir = argv[1];

    QStringList flat;
    QDirIterator flatIt(dir, QDir::Files, QDirIterator::IteratorFlag::NoIteratorFlags);
    while (flatIt.hasNext()) flat << QFileInfo(flatIt.next()).fileName();
    flat.sort();

    QStringList recursive;
    QDirIterator recIt(dir, QDir::Files, QDirIterator::Subdirectories);
    while (recIt.hasNext()) recursive << QFileInfo(recIt.next()).fileName();
    recursive.sort();

    fprintf(stderr, "Non-recursive: %s\n", qPrintable(flat.join(", ")));
    fprintf(stderr, "Recursive:     %s\n", qPrintable(recursive.join(", ")));

    const bool ok = flat == QStringList{"a.mp4", "b.mp3", "c.txt"} &&
                     recursive == QStringList{"a.mp4", "b.mp3", "c.txt", "d.png"};
    fprintf(stderr, "%s\n", ok ? "OK: folder scan behaves as expected" : "FAILED: unexpected scan results");
    return ok ? 0 : 1;
}
