#include <QApplication>

#include "index.h"
#include "zotero.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    const Zotero zotero("/home/khoa/zotero_toy.sqlite3");
    for (const auto &&item : zotero.items())
    {
        item.print();
    }

    const Index index("/home/khoa/index_toy.sqlite3", zotero);
    index.update(true);
    for (auto entry : index.search("Analysis of Positional Encodings for Neural Machine Translation"))
    {
        qWarning() << entry.title << QStringLiteral(" by ") << entry.creators << QStringLiteral(" score ")
                   << entry.score;
    }
}
