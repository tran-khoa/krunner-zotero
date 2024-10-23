#include "index.h"

#include <QFileInfo>

#include "zotero.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include "utils.h"

constexpr int DB_VERSION = 1;


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
            json TEXT DEFAULT "{}"
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
    const auto insertOrReplace =
        QStringLiteral("INSERT OR REPLACE "
                       "INTO search (rowid, key, title, year, creators, authors, editors, tags, "
                       "collections, attachments, notes, abstract) "
                       "VALUES(:rowid, :key, :title, :year, :creators, :authors, :editors, :tags, "
                       ":collections, :attachments, :notes, :abstract);");
    const auto search =
        QStringLiteral("SELECT rowid, *, bm25(search, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.5, 0.4, 0.3, 0.3) "
                       "AS score FROM search WHERE search MATCH ? "
                       "ORDER BY score LIMIT 10");
} // namespace IndexSQL


Index::Index(const QString &dbIndexPath, const QString &dbZoteroPath) :
    m_dbIndexPath(dbIndexPath), m_dbZoteroPath(dbZoteroPath)
{
    m_indexConnectionId = QUuid::createUuid().toString();
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_indexConnectionId);
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
            for (const QString &statement : IndexSQL::reset)
            {
                if (!resetQuery.exec(statement))
                {
                    qWarning() << "Failed to reset Index database: " << resetQuery.lastError().text();
                    m_db.rollback()();
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

    const Zotero zotero(m_dbZoteroPath);
    for (const ZoteroItem &&item : zotero.items(last_modified()))
    {
        const auto creators =
            vectorMapConcat<Creator>(item.creators, [](const auto &creator) { return creator.family; });
        const auto authors =
            vectorMapConcat<Creator>(vectorFilter<Creator>(item.creators, [](const auto &creator)
                                                           { return creator.type == QStringLiteral("author"); }),
                                     [](const auto &creator) { return creator.family; });
        const auto editors =
            vectorMapConcat<Creator>(vectorFilter<Creator>(item.creators, [](const auto &creator)
                                                           { return creator.type == QStringLiteral("editor"); }),
                                     [](const auto &creator) { return creator.family; });
        const auto tags = item.tags.join(QStringLiteral(" "));
        const auto collections =
            vectorMapConcat<Collection>(item.collections, [](const auto &collection) { return collection.name; });
        const auto attachments =
            vectorMapConcat<Attachment>(item.attachments, [](const auto &attachment) { return attachment.title; });
        const auto notes = item.notes.join(QStringLiteral(" "));

        QSqlQuery query(m_db);
        query.prepare(IndexSQL::insertOrReplace);
        query.bindValue(QStringLiteral(":rowid"), item.id);
        query.bindValue(QStringLiteral(":key"), item.key);
        query.bindValue(QStringLiteral(":title"), item.title);
        query.bindValue(QStringLiteral(":year"), item.date.toString(QStringLiteral("yyyy")));
        query.bindValue(QStringLiteral(":creators"), creators);
        query.bindValue(QStringLiteral(":authors"), authors);
        query.bindValue(QStringLiteral(":editors"), editors);
        query.bindValue(QStringLiteral(":tags"), tags);
        query.bindValue(QStringLiteral(":collections"), collections);
        query.bindValue(QStringLiteral(":attachments"), attachments);
        query.bindValue(QStringLiteral(":notes"), notes);
        query.bindValue(QStringLiteral(":abstract"), item.abstract);
        if (!query.exec())
        {
            qWarning() << "Failed to insert or replace item in Index: " << query.lastError().text();
        }
    }
}

std::vector<IndexEntry> Index::search(const QString &needle) const
{
    QSqlQuery query(m_db);
    query.prepare(IndexSQL::search);
    query.addBindValue(needle);
    if (!query.exec())
    {
        qWarning() << "Failed to search Index: " << query.lastError().text();
        return {};
    }
    std::vector<IndexEntry> result;
    while (query.next())
    {
        IndexEntry entry;
        entry.id = query.value(QStringLiteral("rowid")).toInt();
        entry.title = query.value(QStringLiteral("title")).toString();
        entry.year = query.value(QStringLiteral("year")).toString();
        entry.creators = query.value(QStringLiteral("creators")).toString();
        entry.authors = query.value(QStringLiteral("authors")).toString();
        entry.editors = query.value(QStringLiteral("editors")).toString();
        entry.tags = query.value(QStringLiteral("tags")).toString();
        entry.collections = query.value(QStringLiteral("collections")).toString();
        entry.attachments = query.value(QStringLiteral("attachments")).toString();
        entry.notes = query.value(QStringLiteral("notes")).toString();
        entry.abstract = query.value(QStringLiteral("abstract")).toString();
        entry.score = query.value(QStringLiteral("score")).toFloat();
        result.push_back(entry);
    }
    return result;
}
