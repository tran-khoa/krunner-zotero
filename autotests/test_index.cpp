#include <QApplication>

#include "index.h"
#include "zotero.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    const Index index("/home/khoa/index_toy.sqlite3", "/home/khoa/zotero_toy.sqlite3");
    index.update(true);

    std::string name;
    std::cout << "Query? ";
    std::getline(std::cin, name);
    std::cout << "Searching for " << name << std::endl;

    for (const auto &entry : index.search(QString::fromStdString(name)))
    {
        qWarning() << entry.title << QStringLiteral(" by ") << entry.creators << QStringLiteral(" score ")
                   << entry.score;
    }
}
