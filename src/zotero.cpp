#include "zotero.h"
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <generator>
#include <optional>
#include "zotero_item.h"

Q_LOGGING_CATEGORY(KRunnerZoteroZotero, "krunner-zotero/zotero")


namespace ZoteroSQL
{
    const auto query = QStringLiteral(R"(
        WITH _Authors AS (SELECT itemCreators.itemID as parentID,
                                 concat(
                                         creators.firstName, ' ', creators.lastName
                                 )                   as author
                          FROM creators
                                   LEFT JOIN itemCreators ON creators.creatorID = itemCreators.creatorID
                                   LEFT JOIN creatorTypes ON itemCreators.creatorTypeID = creatorTypes.creatorTypeID
                          ORDER BY itemCreators.orderIndex ASC),
             _ItemAuthors AS (SELECT parentID,
                                     json_group_array(author) as authors
                              FROM _Authors
                              GROUP BY parentID),
             _ItemMeta AS (SELECT itemData.itemID as parentID,
                                  json_group_object(
                                          fields.fieldName, itemDataValues.value
                                  )               as meta
                           FROM itemData
                                    LEFT JOIN fields ON itemData.fieldID = fields.fieldID
                                    LEFT JOIN itemDataValues ON itemData.valueID = itemDataValues.valueID
                           GROUP BY itemData.itemID),
             _Attachments AS (SELECT itemAttachments.parentItemID AS parentID,
                                     items.key                    AS key,
                                     itemAttachments.path         AS path,
                                     itemAttachments.contentType  AS contentType,
                                     json_group_object(
                                             fields.fieldName, itemDataValues.value
                                     )                            AS meta
                              FROM itemAttachments
                                       LEFT JOIN items ON itemAttachments.itemID = items.itemID
                                       LEFT JOIN itemData ON items.itemID = itemData.itemID
                                       LEFT JOIN fields ON itemData.fieldID = fields.fieldID
                                       LEFT JOIN itemDataValues ON itemData.valueID = itemDataValues.valueID
                              GROUP BY itemAttachments.itemID),
             _ItemAttachments AS (SELECT parentID,
                                         json_group_array(
                                                 json_patch(
                                                         json_object(
                                                                 'path', path, 'contentType', contentType, 'key', key
                                                         ),
                                                         meta
                                                 )
                                         ) AS attachment_list
                                  from _Attachments
                                  GROUP BY _Attachments.parentID),
             _ItemCollections AS (SELECT collectionItems.itemID                       AS parentID,
                                         json_group_array(collections.collectionName) AS collections
                                  FROM collections
                                           LEFT JOIN collectionItems ON collections.collectionID = collectionItems.collectionID
                                  GROUP BY collectionItems.itemID),
             _ItemNotes AS (SELECT itemNotes.parentItemID           AS parentID,
                                   json_group_array(itemNotes.note) AS note
                            FROM itemNotes
                            GROUP BY itemNotes.parentItemID),
             _ItemTags AS (SELECT itemTags.itemID AS parentID, json_group_array(tags.name) AS tags
                           FROM tags
                                    LEFT JOIN itemTags ON tags.tagID = itemTags.tagID
                           GROUP BY itemTags.itemID)
        SELECT items.itemID                                     AS id,
               items.dateModified                               AS modified,
               items.key                                        AS key,
               coalesce(_ItemAttachments.attachment_list, '[]') AS attachments,
               coalesce(_ItemCollections.collections, '[]')     AS collections,
               _ItemMeta.meta                                   AS meta,
               coalesce(_ItemAuthors.authors, '[]')             AS authors,
               coalesce(_ItemNotes.note, '[]')                  AS note,
               coalesce(_ItemTags.tags, '[]')                   AS tags
        FROM items
                 LEFT JOIN itemTypes ON items.itemTypeID = itemTypes.itemTypeID
                 LEFT JOIN deletedItems ON items.itemID = deletedItems.itemID
                 LEFT JOIN _ItemMeta ON items.itemID = _ItemMeta.parentID
                 LEFT JOIN _ItemAttachments ON items.itemID = _ItemAttachments.parentID
                 LEFT JOIN _ItemCollections ON items.itemID = _ItemCollections.parentID
                 LEFT JOIN _ItemAuthors ON items.itemID = _ItemAuthors.parentID
                 LEFT JOIN _ItemNotes ON items.itemID = _ItemNotes.parentID
                 LEFT JOIN _ItemTags ON items.itemID = _ItemTags.parentID
        WHERE itemTypes.typeName NOT IN ('attachment', 'annotation', 'note')
          AND deletedItems.dateDeleted IS NULL;
        )");
    const auto selectItemsByLastModified = query + QStringLiteral(" AND MODIFIED > ?");
    const auto selectMetadataByID = QStringLiteral(R"(
            SELECT  fields.fieldName AS name,
                    itemDataValues.value AS value
                FROM itemData
                LEFT JOIN fields
                    ON itemData.fieldID = fields.fieldID
                LEFT JOIN itemDataValues
                    ON itemData.valueID = itemDataValues.valueID
                WHERE itemData.itemID = ?
            )");
    const auto queryByLastModified = query + QStringLiteral(" AND MODIFIED > ?");
    const auto queryValidIDs = QStringLiteral(R"(
        SELECT json_group_array(items.itemID) AS id
        FROM items
                 LEFT JOIN itemTypes ON items.itemTypeID = itemTypes.itemTypeID
                 LEFT JOIN deletedItems ON items.itemID = deletedItems.itemID
        WHERE itemTypes.typeName NOT IN ('attachment', 'annotation', 'note')
          AND deletedItems.dateDeleted IS NULL;
        )");
} // namespace ZoteroSQL


QDateTime Zotero::lastModified() const { return QFileInfo(m_dbPath).lastModified(); }

std::vector<int> Zotero::validIDs() const
{
    std::vector<int> ids;
    const auto dbConnectionId = QUuid::createUuid().toString();
    {
        auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), dbConnectionId);
        db.setDatabaseName(m_dbPath);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (!db.open())
        {
            qCCritical(KRunnerZoteroZotero) << "Failed to open Zotero database: " << db.lastError().text();
            return ids;
        }
        QSqlQuery query(db);
        if (!query.exec(ZoteroSQL::queryValidIDs))
        {
            qCCritical(KRunnerZoteroZotero) << "Failed to query valid IDs: " << query.lastError().text();
            return ids;
        }
        if (query.next())
        {
            auto jsonString = query.value(QStringLiteral("id")).toString().toStdString();
            ids = json::parse(jsonString).get<std::vector<int>>();
        }
    }
    QSqlDatabase::removeDatabase(dbConnectionId);
    return ids;
}

std::generator<const ZoteroItem&&> Zotero::items(const std::optional<const QDateTime>& lastModified) const
{
    const auto dbConnectionId = QUuid::createUuid().toString();
    const QString dbCopyPath =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/krunner_zotero_%1.sqlite").
        arg(dbConnectionId);
    if (QFile::exists(dbCopyPath))
    {
        QFile::remove(dbCopyPath);
    }
    QFile::copy(m_dbPath, dbCopyPath);
    {
        auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), dbConnectionId);
        db.setDatabaseName(dbCopyPath);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (!db.open())
        {
            qCCritical(KRunnerZoteroZotero) << "Failed to open Zotero database: " << db.lastError().text();
            QFile::remove(dbCopyPath);
            co_return;
        }
        QSqlQuery query(db);
        bool queryResult;
        if (lastModified.has_value())
        {
            query.prepare(ZoteroSQL::queryByLastModified);
            query.addBindValue(lastModified.value().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
            queryResult = query.exec();
        }
        else
        {
            queryResult = query.exec(ZoteroSQL::query);
        }

        if (!queryResult)
            qCCritical(KRunnerZoteroZotero) << "Failed to query items:" << query.lastError().text();


        while (query.next())
        {
            ZoteroItem item{
                .id = query.value(QStringLiteral("id")).toInt(),
                .key = query.value(QStringLiteral("key")).toString().toStdString(),
                .modified = query.value(QStringLiteral("modified")).toString().toStdString(),
                .meta = json::parse(query.value(QStringLiteral("meta")).toString().toStdString()),
                .attachments = json::parse(query.value(QStringLiteral("attachments")).toString().toStdString()).get<
                    std::vector<Attachment>>(),
                .collections = json::parse(query.value(QStringLiteral("collections")).toString().toStdString()).get<
                    std::vector<std::string>>(),
                .note = json::parse(query.value(QStringLiteral("note")).toString().toStdString()).get<std::vector<
                    std::string>>(),
                .tags = json::parse(query.value(QStringLiteral("tags")).toString().toStdString()).get<std::vector<
                    std::string>>(),
                .authors = json::parse(query.value(QStringLiteral("authors")).toString().toStdString()).get<std::vector<
                    std::string>>()
            };
            co_yield std::move(item);
        }
    }

    QSqlDatabase::removeDatabase(dbConnectionId);
    QFile::remove(dbCopyPath);
}
