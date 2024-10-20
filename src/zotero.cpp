#include "zotero.h"
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <generator>
#include <optional>
#include <qregularexpression.h>

const QRegularExpression ZOTERO_DATE_REGEX(QStringLiteral(R"((\d{4})-(\d{2})-(\d{2}).*)"));

namespace ZoteroSQL
{
    const auto selectItems = QStringLiteral(R"(
    SELECT  items.itemID AS id,
            items.dateModified AS modified,
            items.key AS key,
            items.libraryID AS library,
            itemTypes.typeName AS type
        FROM items
        LEFT JOIN itemTypes
            ON items.itemTypeID = itemTypes.itemTypeID
        LEFT JOIN deletedItems
            ON items.itemID = deletedItems.itemID
        WHERE type NOT IN ("attachment", "note")
        AND deletedItems.dateDeleted IS NULL
    )");
    const auto selectItemsByLastModified = selectItems + QStringLiteral(" AND MODIFIED > ?");
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
    const auto selectAttachmentsByID = QStringLiteral(R"(
        ATTACHMENTS_SQL = u"""
        SELECT
            items.key AS key,
            itemAttachments.path AS path,
            (SELECT  itemDataValues.value
                FROM itemData
                LEFT JOIN fields
                    ON itemData.fieldID = fields.fieldID
                LEFT JOIN itemDataValues
                    ON itemData.valueID = itemDataValues.valueID
            WHERE itemData.itemID = items.itemID AND fields.fieldName = 'title')
            title,
            (SELECT  itemDataValues.value
                FROM itemData
                LEFT JOIN fields
                    ON itemData.fieldID = fields.fieldID
                LEFT JOIN itemDataValues
                    ON itemData.valueID = itemDataValues.valueID
            WHERE itemData.itemID = items.itemID AND fields.fieldName = 'url')
            url
        FROM itemAttachments
            LEFT JOIN items
                ON itemAttachments.itemID = items.itemID
        WHERE itemAttachments.parentItemID = ?
        """)");
    const auto selectCollectionsByID = QStringLiteral(R"(
        SELECT  collections.collectionName AS name,
                collections.key AS key
            FROM collections
            LEFT JOIN collectionItems
                ON collections.collectionID = collectionItems.collectionID
        WHERE collectionItems.itemID = ?
    )");
    const auto selectCreatorsByID = QStringLiteral(R"(
        SELECT  creators.firstName AS given,
                creators.lastName AS family,
                itemCreators.orderIndex AS `index`,
                creatorTypes.creatorType AS `type`
            FROM creators
            LEFT JOIN itemCreators
                ON creators.creatorID = itemCreators.creatorID
            LEFT JOIN creatorTypes
                ON itemCreators.creatorTypeID = creatorTypes.creatorTypeID
        WHERE itemCreators.itemID = ?
        ORDER BY `index` ASC
    )");
    const auto selectNotesByID = QStringLiteral(R"(
        SELECT itemNotes.note AS note
            FROM itemNotes
            LEFT JOIN items
                ON itemNotes.itemID = items.itemID
        WHERE itemNotes.parentItemID = ?
    )");
    const auto selectTagsByID = QStringLiteral(R"(
        SELECT tags.name AS name
            FROM tags
            LEFT JOIN itemTags
                ON tags.tagID = itemTags.tagID
        WHERE itemTags.itemID = ?
    )");
} // namespace ZoteroSQL


Zotero::Zotero(const QString &dbPath) : m_dbPath(dbPath)
{
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("zotero"));
    m_db.setDatabaseName(dbPath);
    m_db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    if (!m_db.open())
    {
        qWarning() << "Failed to open Zotero database";
    }
}

Zotero::~Zotero() { m_db.close(); }

QDateTime Zotero::lastModified() const { return QFileInfo(m_dbPath).lastModified(); }
std::generator<const ZoteroItem &&> Zotero::items(const std::optional<const QDateTime> &lastModified) const
{
    QSqlQuery itemQuery(m_db);
    bool queryResult;
    if (lastModified.has_value())
    {
        itemQuery.prepare(ZoteroSQL::selectItemsByLastModified);
        itemQuery.addBindValue(lastModified.value().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        queryResult = itemQuery.exec();
    }
    else
    {
        queryResult = itemQuery.exec(ZoteroSQL::selectItems);
    }

    if (!queryResult)
    {
        qWarning() << "Failed to query items: " << itemQuery.lastError().text();
    }

    while (itemQuery.next())
    {
        ZoteroItem item;
        item.id = itemQuery.value(QStringLiteral("id")).toInt();
        item.modified = itemQuery.value(QStringLiteral("modified")).toDateTime();
        item.key = itemQuery.value(QStringLiteral("key")).toString();
        item.library = itemQuery.value(QStringLiteral("library")).toInt();
        item.type = itemQuery.value(QStringLiteral("type")).toString();

        QSqlQuery metadataQuery(m_db);
        metadataQuery.prepare(ZoteroSQL::selectMetadataByID);
        metadataQuery.addBindValue(item.id);
        metadataQuery.exec();
        while (metadataQuery.next())
        {
            const QString name = metadataQuery.value(QStringLiteral("name")).toString();
            const QString value = metadataQuery.value(QStringLiteral("value")).toString();

            if (name == QStringLiteral("title") || name == QStringLiteral("caseName"))
            {
                item.title = value;
            }
            else if (name == QStringLiteral("date"))
            {

                QRegularExpressionMatch match = ZOTERO_DATE_REGEX.match(value);
                if (!match.hasMatch())
                {
                    item.date = QDate(value.left(4).toInt(), 1, 1);
                }
                else
                {
                    item.date = QDate(match.captured(1).toInt(), match.captured(2).toInt(), match.captured(3).toInt());
                }
            }
            else if (name == QStringLiteral("abstractNote"))
            {
                item.abstract = value;
            }
        }

        QSqlQuery attachmentsQuery(m_db);
        attachmentsQuery.prepare(ZoteroSQL::selectAttachmentsByID);
        attachmentsQuery.addBindValue(item.id);
        attachmentsQuery.exec();
        while (attachmentsQuery.next())
        {
            Attachment attachment;
            attachment.key = attachmentsQuery.value(QStringLiteral("key")).toString();
            attachment.path = attachmentsQuery.value(QStringLiteral("path")).toString();
            attachment.title = attachmentsQuery.value(QStringLiteral("title")).toString();
            attachment.url = attachmentsQuery.value(QStringLiteral("url")).toString();

            item.attachments.append(std::move(attachment));
        }

        QSqlQuery collectionsQuery(m_db);
        collectionsQuery.prepare(ZoteroSQL::selectCollectionsByID);
        collectionsQuery.addBindValue(item.id);
        collectionsQuery.exec();
        while (collectionsQuery.next())
        {
            Collection collection;
            collection.name = collectionsQuery.value(QStringLiteral("name")).toString();
            collection.key = collectionsQuery.value(QStringLiteral("key")).toString();

            item.collections.append(std::move(collection));
        }

        QSqlQuery creatorsQuery(m_db);
        creatorsQuery.prepare(ZoteroSQL::selectCreatorsByID);
        creatorsQuery.addBindValue(item.id);
        creatorsQuery.exec();
        while (creatorsQuery.next())
        {
            Creator creator;
            creator.index = creatorsQuery.value(QStringLiteral("index")).toInt();
            creator.given = creatorsQuery.value(QStringLiteral("given")).toString();
            creator.family = creatorsQuery.value(QStringLiteral("family")).toString();
            creator.type = creatorsQuery.value(QStringLiteral("type")).toString();

            item.creators.append(std::move(creator));
        }

        QSqlQuery notesQuery(m_db);
        notesQuery.prepare(ZoteroSQL::selectNotesByID);
        notesQuery.addBindValue(item.id);
        notesQuery.exec();
        while (notesQuery.next())
        {
            auto note = notesQuery.value(QStringLiteral("note")).toString();
            note.remove(QRegularExpression(QStringLiteral(R"(<[^>]*>)"))); // Remove HTML tags
            item.notes.append(note.simplified());
        }

        QSqlQuery tagsQuery(m_db);
        tagsQuery.prepare(ZoteroSQL::selectTagsByID);
        tagsQuery.addBindValue(item.id);
        tagsQuery.exec();
        while (tagsQuery.next())
        {
            item.tags.append(tagsQuery.value(QStringLiteral("name")).toString());
        }


        co_yield std::move(item);
    }
}

