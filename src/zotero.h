#pragma once

#include <QDateTime>
#include <QSqlDatabase>
#include <QString>
#include <generator>
#include "zotero.pb.h"


// struct Attachment
// {
//     QString key;
//     QString path;
//     QString title;
//     QString url;
// };
//
// struct Collection
// {
//     QString name;
//     QString key;
// };
// struct Creator
// {
//     int index;
//     QString given;
//     QString family;
//     QString type;
// };
//
// struct ZoteroItem
// {
//     int id;
//     QDateTime modified;
//     QString key;
//     int library;
//     QString type;
//
//     QString title;
//     QDate date;
//     QString abstract;
//     QVector<Attachment> attachments;
//     QVector<Collection> collections;
//     QVector<Creator> creators;
//     QVector<QString> notes;
//     QVector<QString> tags;
// };

class Zotero
{
public:
    explicit Zotero(const QString &dbPath);
    ~Zotero();
    [[nodiscard]] QDateTime lastModified() const;
    [[nodiscard]] std::generator<const ZoteroItem>
    items(const std::optional<const QDateTime> &lastModified = std::nullopt) const;


private:
    QString m_dbPath;
    QSqlDatabase m_db;
    QString m_dbConnectionId;
};
