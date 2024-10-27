#include <QApplication>
#include <iostream>
#include "zotero.h"
using json = nlohmann::json;

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    Zotero zotero(QStringLiteral("/home/work/zotero_toy.sql"));
    for (const auto &&item : zotero.items())
    {
        json j = item;
        std::cout << j << std::endl;
    }
}
