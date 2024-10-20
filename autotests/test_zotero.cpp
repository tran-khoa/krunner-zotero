#include <QApplication>
#include "zotero.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    Zotero zotero("/home/khoa/zotero_toy.sql");
    for (const auto &&item : zotero.items())
    {
        item.print();
    }
}
