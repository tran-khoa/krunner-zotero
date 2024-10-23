#pragma once

#include <QDateTime>
#include <QSqlDatabase>
#include <QString>
#include <generator>
#include <iostream>

struct Attachment
{
    QString key;
    QString path;
    QString title;
    QString url;
};

struct Collection
{
    QString name;
    QString key;
};
struct Creator
{
    int index;
    QString given;
    QString family;
    QString type;
};

struct ZoteroItem
{
    int id;
    QDateTime modified;
    QString key;
    int library;
    QString type;

    QString title;
    QDate date;
    QString abstract;
    QVector<Attachment> attachments;
    QVector<Collection> collections;
    QVector<Creator> creators;
    QVector<QString> notes;
    QVector<QString> tags;

    void print() const
    {
        std::cout << "id=" << id << std::endl;
        std::cout << "modified=" << modified.toString().toStdString() << std::endl;
        std::cout << "key=" << key.toStdString() << std::endl;
        std::cout << "library=" << library << std::endl;
        std::cout << "type=" << type.toStdString() << std::endl;
        std::cout << "title=" << title.toStdString() << std::endl;
        std::cout << "date=" << date.toString().toStdString() << std::endl;
        std::cout << "abstract=" << abstract.toStdString() << std::endl;
        for (const auto &attachment : attachments)
        {
            std::cout << "attachment key=" << attachment.key.toStdString() << std::endl;
            std::cout << "attachment path=" << attachment.path.toStdString() << std::endl;
            std::cout << "attachment title=" << attachment.title.toStdString() << std::endl;
            std::cout << "attachment url=" << attachment.url.toStdString() << std::endl;
        }
        for (const auto &collection : collections)
        {
            std::cout << "collection name=" << collection.name.toStdString() << std::endl;
            std::cout << "collection key=" << collection.key.toStdString() << std::endl;
        }
        for (const auto &creator : creators)
        {
            std::cout << "creator index=" << creator.index << std::endl;
            std::cout << "creator given=" << creator.given.toStdString() << std::endl;
            std::cout << "creator family=" << creator.family.toStdString() << std::endl;
            std::cout << "creator type=" << creator.type.toStdString() << std::endl;
        }
        for (const auto &note : notes)
        {
            std::cout << "note=" << note.toStdString() << std::endl;
        }
        for (const auto &tag : tags)
        {
            std::cout << "tag=" << tag.toStdString() << std::endl;
        }
    }
};

class Zotero
{
public:
    explicit Zotero(const QString &dbPath);
    ~Zotero();
    [[nodiscard]] QDateTime lastModified() const;
    [[nodiscard]] std::generator<const ZoteroItem &&>
    items(const std::optional<const QDateTime> &lastModified = std::nullopt) const;


private:
    QString m_dbPath;
    QSqlDatabase m_db;
    QString m_dbConnectionId;
};
