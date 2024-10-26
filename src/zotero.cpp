#include "zotero.h"
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <generator>
#include <optional>

#include "zotero.pb.h"

const QRegularExpression ZOTERO_DATE_REGEX(QStringLiteral(R"((\d{4})-(\d{2})-(\d{2}).*)"));
const QRegularExpression HTML_TAG_REGEX(QStringLiteral(R"(<[^>]*>)"));

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
        WHERE type NOT IN ("attachment", "annotation", "note")
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
        SELECT
            items.key AS key,
            itemAttachments.path AS path,
            itemAttachments.contentType AS contentType,
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
    )");
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
    m_dbConnectionId = QUuid::createUuid().toString();
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_dbConnectionId);
    QString tempFile =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/zotero.sqlite");
    if (QFile::exists(tempFile))
    {
        QFile::remove(tempFile);
    }
    QFile::copy(dbPath, tempFile);
    m_db.setDatabaseName(tempFile);
    m_db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    if (!m_db.open())
    {
        qWarning() << "Failed to open Zotero database: " << m_db.lastError().text();
    }
}

Zotero::~Zotero()
{
    m_db.close();
    QSqlDatabase::removeDatabase(m_dbConnectionId);
    qDebug() << "Zotero database closed";
}

QDateTime Zotero::lastModified() const { return QFileInfo(m_dbPath).lastModified(); }
std::generator<const ZoteroItem> Zotero::items(const std::optional<const QDateTime> &lastModified) const
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
        qWarning() << "Failed to query items:" << itemQuery.lastError().text();
    }

    while (itemQuery.next())
    {
        ZoteroItem item;
        item.set_id(itemQuery.value(QStringLiteral("id")).toInt());
        item.set_modified(itemQuery.value(QStringLiteral("modified"))
                              .toDateTime()
                              .toString(QStringLiteral("yyyy-MM-dd"))
                              .toStdString());
        item.set_key(itemQuery.value(QStringLiteral("key")).toString().toStdString());
        item.set_library(itemQuery.value(QStringLiteral("library")).toInt());
        item.set_type(itemQuery.value(QStringLiteral("type")).toString().toStdString());

        QSqlQuery metadataQuery(m_db);
        metadataQuery.prepare(ZoteroSQL::selectMetadataByID);
        metadataQuery.addBindValue(item.id());
        metadataQuery.exec();
        if (metadataQuery.lastError().isValid())
        {
            qWarning() << "Failed to query metadata: " << metadataQuery.lastError().text();
        }
        while (metadataQuery.next())
        {
            const QString name = metadataQuery.value(QStringLiteral("name")).toString();
            const QString value = metadataQuery.value(QStringLiteral("value")).toString();

            if (name == QStringLiteral("title") || name == QStringLiteral("caseName"))
            {
                item.set_title(value.toStdString());
            }
            else if (name == QStringLiteral("date"))
            {

                QRegularExpressionMatch match = ZOTERO_DATE_REGEX.match(value);
                if (!match.hasMatch())
                {
                    item.set_date(value.left(4).toStdString());
                }
                else
                {
                    item.set_date(match.captured(1).toStdString());
                }
            }
            else if (name == QStringLiteral("abstractNote"))
            {
                item.set_abstract(value.toStdString());
            }
        }

        QSqlQuery attachmentsQuery(m_db);
        attachmentsQuery.prepare(ZoteroSQL::selectAttachmentsByID);
        attachmentsQuery.addBindValue(item.id());
        attachmentsQuery.exec();
        if (attachmentsQuery.lastError().isValid())
        {
            qWarning() << "Failed to query attachments: " << attachmentsQuery.lastError().text();
        }
        while (attachmentsQuery.next())
        {
            Attachment *attachment = item.add_attachments();
            attachment->set_key(attachmentsQuery.value(QStringLiteral("key")).toString().toStdString());
            attachment->set_path(attachmentsQuery.value(QStringLiteral("path")).toString().toStdString());
            attachment->set_title(attachmentsQuery.value(QStringLiteral("title")).toString().toStdString());
            attachment->set_url(attachmentsQuery.value(QStringLiteral("url")).toString().toStdString());
            attachment->set_contenttype(attachmentsQuery.value(QStringLiteral("contentType")).toString().toStdString());
        }

        QSqlQuery collectionsQuery(m_db);
        collectionsQuery.prepare(ZoteroSQL::selectCollectionsByID);
        collectionsQuery.addBindValue(item.id());
        collectionsQuery.exec();
        if (collectionsQuery.lastError().isValid())
        {
            qWarning() << "Failed to query collections: " << collectionsQuery.lastError().text();
        }
        while (collectionsQuery.next())
        {
            Collection *collection = item.add_collections();
            collection->set_name(collectionsQuery.value(QStringLiteral("name")).toString().toStdString());
            collection->set_key(collectionsQuery.value(QStringLiteral("key")).toString().toStdString());
        }

        QSqlQuery creatorsQuery(m_db);
        creatorsQuery.prepare(ZoteroSQL::selectCreatorsByID);
        creatorsQuery.addBindValue(item.id());
        creatorsQuery.exec();
        if (creatorsQuery.lastError().isValid())
        {
            qWarning() << "Failed to query creators: " << creatorsQuery.lastError().text();
        }
        while (creatorsQuery.next())
        {
            Creator *creator = item.add_creators();
            creator->set_index(creatorsQuery.value(QStringLiteral("index")).toInt());
            creator->set_given(creatorsQuery.value(QStringLiteral("given")).toString().toStdString());
            creator->set_family(creatorsQuery.value(QStringLiteral("family")).toString().toStdString());
            creator->set_type(creatorsQuery.value(QStringLiteral("type")).toString().toStdString());
        }

        QSqlQuery notesQuery(m_db);
        notesQuery.prepare(ZoteroSQL::selectNotesByID);
        notesQuery.addBindValue(item.id());
        notesQuery.exec();
        if (notesQuery.lastError().isValid())
        {
            qWarning() << "Failed to query notes: " << notesQuery.lastError().text();
        }
        while (notesQuery.next())
        {
            auto note = notesQuery.value(QStringLiteral("note")).toString();
            note.remove(HTML_TAG_REGEX);
            item.add_notes(note.simplified().toStdString());
        }

        QSqlQuery tagsQuery(m_db);
        tagsQuery.prepare(ZoteroSQL::selectTagsByID);
        tagsQuery.addBindValue(item.id());
        tagsQuery.exec();
        if (tagsQuery.lastError().isValid())
        {
            qWarning() << "Failed to query tags: " << tagsQuery.lastError().text();
        }
        while (tagsQuery.next())
        {
            item.add_tags(tagsQuery.value(QStringLiteral("name")).toString().toStdString());
        }

        if (!item.IsInitialized())
        {
            qWarning() << "Item is not initialized: " << item.DebugString().c_str();
        }

        co_yield item;
    }
}
