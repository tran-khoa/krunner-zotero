#include <QApplication>

#include <iostream>
#include "index.h"
#include "zotero.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    Zotero zotero(QStringLiteral("/home/work/zotero_toy.sql"));
    const Index index(QStringLiteral("/home/work/index_toy.sql"), std::move(zotero));
    // ReSharper disable once CppExpressionWithoutSideEffects
    index.setup();
    index.update();

    std::string name;
    std::cout << "Query? ";
    std::getline(std::cin, name);
    std::cout << "Searching for " << name << std::endl;

    for (const auto &[item, score] : index.search(QString::fromStdString(name)))
    {
        qWarning() << item.key << QStringLiteral(" score ") << score;
    }
}
