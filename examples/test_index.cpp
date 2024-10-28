#include <QApplication>

#include "index.h"
#include "zotero.h"
#include <iostream>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const Zotero zotero(QStringLiteral("/home/work/zotero_toy.sql"));
    auto index = Index(QStringLiteral("/home/work/index_toy.sql"), zotero);
    // ReSharper disable once CppExpressionWithoutSideEffects
    index.setup();
    index.update();

    std::string name;
    std::cout << "Query? ";
    std::getline(std::cin, name);
    std::cout << "Searching for " << name << std::endl;

    for (const auto &[item, score] : index.search(QString::fromStdString(name))) {
        qWarning() << item.key << QStringLiteral(" score ") << score;
        for (const auto &attachment : item.attachments) {
            if (attachment.contentType == "application/pdf") {
                std::cout << attachment.key << std::endl;
            }
        }
    }
}
