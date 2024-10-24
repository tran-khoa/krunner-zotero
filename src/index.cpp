#include "index.h"

#include <QFileInfo>

#include "zotero.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include "utils.h"
#include "zotero.pb.h"

constexpr int DB_VERSION = 0;


namespace IndexSQL
{
    const std::array createTables = {

        QStringLiteral(R"(
        CREATE VIRTUAL TABLE search USING fts5(
            key,
            title,
            year,
            creators,
            authors,
            editors,
            tags,
            collections,
            attachments,
            notes,
            abstract
        );
    )"),
        QStringLiteral(R"(
        CREATE TABLE modified (
            id INTEGER PRIMARY KEY NOT NULL,
            modified TIMESTAMP NOT NULL
        );
)"),
        QStringLiteral(R"(
        CREATE TABLE data (
            id INTEGER PRIMARY KEY NOT NULL,
            obj TEXT DEFAULT "{}"
        );
)"),
        QStringLiteral(R"(
        CREATE TABLE dbinfo (
            key TEXT PRIMARY KEY NOT NULL,
            value TEXT NOT NULL
        );
    )"),
        QStringLiteral("INSERT INTO dbinfo VALUES('version', %1);").arg(DB_VERSION)};
    const auto getVersion = QStringLiteral("SELECT value AS version FROM dbinfo WHERE key = 'version'");
    const std::array reset = {QStringLiteral(R"(DROP TABLE IF EXISTS `data`;)"),
                              QStringLiteral(R"(DROP TABLE IF EXISTS `dbinfo`;)"),
                              QStringLiteral(R"(DROP TABLE IF EXISTS `modified`;)"),
                              QStringLiteral(R"(DROP TABLE IF EXISTS `search`;)"),
                              QStringLiteral(R"(VACUUM;)"),
                              QStringLiteral(R"(PRAGMA INTEGRITY_CHECK;)")};
    const auto insertOrReplaceSearch =
        QStringLiteral("INSERT OR REPLACE "
                       "INTO search (rowid, key, title, year, creators, authors, editors, tags, "
                       "collections, attachments, notes, abstract) "
                       "VALUES(:rowid, :key, :title, :year, :creators, :authors, :editors, :tags, "
                       ":collections, :attachments, :notes, :abstract);");
    const auto insertOrReplaceData = QStringLiteral("INSERT OR REPLACE INTO data (id, obj) VALUES(:id, :obj);");
    const auto search =
        QStringLiteral("SELECT rowid, *, bm25(search, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.5, 0.4, 0.3, 0.3) "
                       "AS score FROM search WHERE search MATCH ? "
                       "ORDER BY score LIMIT 10");
    const auto selectData = QStringLiteral("SELECT obj FROM data WHERE id = ?");
} // namespace IndexSQL


Index::Index(const QString &dbIndexPath, const QString &dbZoteroPath) :
    m_dbIndexPath(dbIndexPath), m_dbZoteroPath(dbZoteroPath)
{
    m_indexConnectionId = QUuid::createUuid().toString();
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_indexConnectionId);
    qDebug() << "New connection id: " << m_indexConnectionId;
    m_db.setDatabaseName(m_dbIndexPath);
    if (!m_db.open())
    {
        qWarning() << "Failed to open Index database: " << m_db.lastError().text();
        return;
    }
    setup();
    update();
}
Index::~Index()
{
    m_db.close();
    QSqlDatabase::removeDatabase(m_indexConnectionId);
}

void Index::setup()
{
    QSqlQuery versionQuery(m_db);
    versionQuery.exec(IndexSQL::getVersion);
    if (versionQuery.next())
    {
        if (versionQuery.value(QStringLiteral("version")).toInt() != DB_VERSION)
        {
            qDebug() << "[Index] Database version mismatch: expected " << DB_VERSION << " but got "
                     << versionQuery.value(QStringLiteral("version")).toInt();
            QSqlQuery resetQuery(m_db);
            m_db.transaction();
            for (const QString &statement : IndexSQL::reset)
            {
                if (!resetQuery.exec(statement))
                {
                    qWarning() << "Failed to reset Index database: " << resetQuery.lastError().text();
                    m_db.rollback();
                    return;
                }
            }
            m_db.commit();
        }
        else
        {
            return;
        }
    }
    qDebug() << "[Index] Creating tables";
    QSqlQuery createQuery(m_db);
    m_db.transaction();
    for (const QString &statement : IndexSQL::createTables)
    {
        if (!createQuery.exec(statement))
        {
            qWarning() << "Failed to create tables for Index: " << createQuery.lastError().text();
            m_db.rollback();
            return;
        }
    }
    m_db.commit();
    update(true);
}

QDateTime Index::last_modified() const { return QFileInfo(m_dbIndexPath).lastModified(); }


bool Index::needs_update() const { return QFileInfo(m_dbZoteroPath).lastModified() > last_modified(); }

void Index::update(const bool force) const
{
    if (!force && !needs_update())
    {
        qDebug() << "[Index] Not updating index";
        return;
    }

    qDebug() << "[Index] Updating index";

    for (const Zotero zotero(m_dbZoteroPath); const ZoteroItem &item : zotero.items(last_modified()))
    {
        const auto creators = repeatedFieldMapConcat<Creator>(item.creators(), [](const auto &creator)
                                                              { return QString::fromStdString(creator.family()); });
        const auto authors = repeatedFieldMapConcat<Creator>(
            repeatedFieldFilter<Creator>(item.creators(), [](const auto &creator)
                                         { return creator.type() == QStringLiteral("author"); }),
            [](const auto &creator) { return QString::fromStdString(creator.family()); });
        const auto editors = repeatedFieldMapConcat<Creator>(
            repeatedFieldFilter<Creator>(item.creators(), [](const auto &creator)
                                         { return creator.type() == QStringLiteral("editor"); }),
            [](const auto &creator) { return QString::fromStdString(creator.family()); });
        const auto tags = repeatedFieldMapConcat<std::string>(item.tags(), [](const auto &tag)
                                                              { return QString::fromStdString(tag); });
        const auto collections = repeatedFieldMapConcat<Collection>(
            item.collections(), [](const auto &collection) { return QString::fromStdString(collection.name()); });
        const auto attachments = repeatedFieldMapConcat<Attachment>(
            item.attachments(), [](const auto &attachment) { return QString::fromStdString(attachment.title()); });
        const auto notes = repeatedFieldMapConcat<std::string>(item.notes(), [](const auto &note)
                                                               { return QString::fromStdString(note); });

        QSqlQuery query(m_db);
        query.prepare(IndexSQL::insertOrReplaceSearch);
        query.bindValue(QStringLiteral(":rowid"), item.id());
        query.bindValue(QStringLiteral(":key"), QString::fromStdString(item.key()));
        query.bindValue(QStringLiteral(":title"), QString::fromStdString(item.title()));
        query.bindValue(QStringLiteral(":year"), QString::fromStdString(item.date()));
        query.bindValue(QStringLiteral(":creators"), creators);
        query.bindValue(QStringLiteral(":authors"), authors);
        query.bindValue(QStringLiteral(":editors"), editors);
        query.bindValue(QStringLiteral(":tags"), tags);
        query.bindValue(QStringLiteral(":collections"), collections);
        query.bindValue(QStringLiteral(":attachments"), attachments);
        query.bindValue(QStringLiteral(":notes"), notes);
        query.bindValue(QStringLiteral(":abstract"), QString::fromStdString(item.abstract()));
        if (!query.exec())
        {
            qWarning() << "Failed to insert or replace item in Index: " << query.lastError().text();
        }

        QSqlQuery insertDataQuery(m_db);
        insertDataQuery.prepare(IndexSQL::insertOrReplaceData);
        insertDataQuery.bindValue(QStringLiteral(":id"), item.id());
        std::string serializedItem;
        if (!item.SerializeToString(&serializedItem))
        {
            qDebug() << "Failed to serialize data." << item.DebugString().c_str();
            return;
        }
        insertDataQuery.bindValue(QStringLiteral(":obj"),
                                  QByteArray(serializedItem.data(), static_cast<int>(serializedItem.size())));
        if (!insertDataQuery.exec())
        {
            qWarning() << "Failed to insert or replace data in Index: " << insertDataQuery.lastError().text();
        }
    }
}

std::vector<std::pair<ZoteroItem, float>> Index::search(const QString &needle) const
{
    QSqlQuery query(m_db);
    query.prepare(IndexSQL::search);
    query.addBindValue(needle);
    if (!query.exec())
    {
        qWarning() << "Failed to search Index: " << query.lastError().text();
        return {};
    }
    std::vector<std::pair<ZoteroItem, float>> result;
    while (query.next())
    {
        const auto id = query.value(QStringLiteral("rowid")).toInt();
        auto score = query.value(QStringLiteral("score")).toFloat();
        QSqlQuery dataQuery(m_db);
        dataQuery.prepare(IndexSQL::selectData);
        dataQuery.addBindValue(id);
        dataQuery.exec();
        if (!dataQuery.exec())
        {
            qWarning() << "Failed to get data for item " << id << ": " << dataQuery.lastError().text();
            continue;
        }
        if (dataQuery.next())
        {
            QByteArray data = dataQuery.value(QStringLiteral("obj")).toByteArray();
            ZoteroItem item;
            if (!item.ParseFromArray(data.data(), static_cast<int>(data.size())))
            {
                qWarning() << "Failed to parse item " << id;
                continue;
            }
            result.emplace_back(item, score);
        }
        else
        {
            qWarning() << "Failed to parse item " << id << ": no data";
        }
    }
    return result;
}
