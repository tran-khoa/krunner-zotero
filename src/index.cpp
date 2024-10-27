#include "index.h"

#include <QFileInfo>

#include "zotero.h"

#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include "utils.h"

using json = nlohmann::json;


constexpr int DB_VERSION = 0;

const QRegularExpression ZOTERO_DATE_REGEX(QStringLiteral(R"((\d{4})-(\d{2})-(\d{2}).*)"));
const QRegularExpression HTML_TAG_REGEX(QStringLiteral(R"(<[^>]*>)"));

template <typename T>
std::string join(const std::vector<T> &vec, const char sep = ' ')
{
    std::ostringstream oss;
    for (size_t i = 0; i < vec.size(); ++i)
    {
        oss << vec[i];
        if (i != vec.size() - 1)
        {
            oss << sep;
        }
    }
    return oss.str();
}


namespace IndexSQL
{
    const std::array createTables = {
        QStringLiteral(R"(
        CREATE VIRTUAL TABLE search USING fts5(
            key,
            title,
            shortTitle,
            doi,
            year,
            authors,
            tags,
            collections,
            notes,
            abstract,
            publisher
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
        QStringLiteral("INSERT INTO dbinfo VALUES('version', %1);").arg(DB_VERSION)
    };
    const auto getVersion = QStringLiteral("SELECT value AS version FROM dbinfo WHERE key = 'version'");
    const std::array reset = {QStringLiteral("DROP TABLE IF EXISTS `data`;"),
                              QStringLiteral("DROP TABLE IF EXISTS `dbinfo`;"),
                              QStringLiteral("DROP TABLE IF EXISTS `modified`;"),
                              QStringLiteral("DROP TABLE IF EXISTS `search`;"),
                              QStringLiteral("VACUUM;"),
                              QStringLiteral("PRAGMA INTEGRITY_CHECK;")};
    const auto insertOrReplaceSearch =
        QStringLiteral("INSERT OR REPLACE "
            "INTO search (rowid, key, title, shortTitle, doi, year, authors, tags, collections, notes, abstract, publisher) "
            "VALUES(:rowid, :key, :title, :shortTitle, :doi, :year, :authors, :tags, :collections, :notes, :abstract, :publisher);");
    const auto insertOrReplaceData = QStringLiteral("INSERT OR REPLACE INTO data (id, obj) VALUES(:id, :obj);");
    const auto search =
        QStringLiteral("SELECT rowid, *, bm25(search, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.7, 0.5, 0.4, 0.4, 0.4) "
            "AS score FROM search WHERE search MATCH ? "
            "ORDER BY score LIMIT 10");
    const auto selectData = QStringLiteral("SELECT obj FROM data WHERE id = ?");
    const auto deleteIdNotIn = QStringLiteral("DELETE FROM data WHERE id NOT IN (?);");
} // namespace IndexSQL


bool Index::setup() const
{
    /**
    * @brief Setup the index database
    *
    * @return true if the database was (re-)created, false otherwise
    */
    bool do_update = false;
    const auto connectionId = QUuid::createUuid().toString();
    {
        auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionId);
        db.setDatabaseName(m_dbIndexPath);
        if (!db.open())
        {
            qWarning() << "Failed to open Index database: " << db.lastError().text();
            return false;
        }

        bool do_create_tables = true;

        QSqlQuery versionQuery(db);
        versionQuery.exec(IndexSQL::getVersion);
        if (versionQuery.next())
        {
            if (versionQuery.value(QStringLiteral("version")).toInt() != DB_VERSION)
            {
                qWarning() << "[Index] Database version outdated, most recent version is: " << DB_VERSION << " but is "
                    << versionQuery.value(QStringLiteral("version")).toInt();

                if (db.transaction())
                {
                    QSqlQuery resetQuery(db);
                    for (const QString &statement : IndexSQL::reset)
                    {
                        resetQuery.exec(statement);
                    }
                    if (!db.commit())
                    {
                        qWarning() << "[Index] Failed to commit reset" << db.lastError().text();
                        db.rollback();
                        do_create_tables = false;
                    }
                    else
                    {
                        qDebug() << "[Index] Reset completed";
                    }
                }
                else
                {
                    qWarning() << "[Index] Failed to start transaction" << db.lastError().text();
                    do_create_tables = false;
                }
            }
            else
            {
                do_create_tables = false;
            }
        }

        if (do_create_tables)
        {
            qDebug() << "[Index] Creating tables...";
            if (db.transaction())
            {
                QSqlQuery createQuery(db);
                for (const QString &statement : IndexSQL::createTables)
                {
                    createQuery.exec(statement);
                }
                if (!db.commit())
                {
                    qWarning() << "[Index] Failed to commit create tables" << db.lastError().text();
                    db.rollback();
                }
                else
                {
                    qDebug() << "[Index] Tables created";
                    do_update = true;
                }
            }
            else
            {
                qWarning() << "[Index] Failed to start transaction" << db.lastError().text();
            }
        }
    }
    QSqlDatabase::removeDatabase(connectionId);

    if (do_update)
        update(true);
    return do_update;
}

QDateTime Index::last_modified() const { return QFileInfo(m_dbIndexPath).lastModified(); }

bool Index::needs_update() const { return m_zotero.lastModified() > last_modified(); }

QVariant getOrNull(const std::unordered_map<std::string, std::string> &m, const std::string &key)
{
    if (const auto it = m.find(key); it != m.end())
    {
        return QString::fromStdString(it->second);
    }
    return {};
}

void Index::update(const bool force) const
{
    if (!force && !needs_update())
    {
        qDebug() << "[Index] Index is up to date.";
        return;
    }

    qDebug() << "[Index] Updating index...";
    const auto connectionId = QUuid::createUuid().toString();
    {
        auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionId);
        db.setDatabaseName(m_dbIndexPath);
        if (!db.open())
        {
            qWarning() << "[Index] Failed to open Index database: " << db.lastError().text();
            return;
        }

        for (const ZoteroItem &&item : m_zotero.items(last_modified()))
        {
            if (db.transaction())
            {
                QSqlQuery metaQuery(db);
                metaQuery.prepare(IndexSQL::insertOrReplaceSearch);

                metaQuery.bindValue(QStringLiteral(":rowid"), item.id);
                metaQuery.bindValue(QStringLiteral(":key"), QString::fromStdString(item.key));
                metaQuery.bindValue(QStringLiteral(":title"), getOrNull(item.meta, "title"));
                metaQuery.bindValue(QStringLiteral(":shortTitle"), getOrNull(item.meta, "shortTitle"));
                metaQuery.bindValue(QStringLiteral(":doi"), getOrNull(item.meta, "DOI"));
                metaQuery.bindValue(QStringLiteral(":abstract"), getOrNull(item.meta, "abstractNote"));
                metaQuery.bindValue(QStringLiteral(":year"), QVariant());
                for (const auto dateKey : {"dateEnacted", "dateDecided", "filingDate", "issueDate", "date"})
                {
                    if (const auto it = item.meta.find(dateKey); it != item.meta.end())
                    {
                        const auto dateValue = QString::fromStdString(it->second);
                        const QRegularExpressionMatch match = ZOTERO_DATE_REGEX.match(dateValue);
                        metaQuery.bindValue(
                            QStringLiteral(":year"), match.hasMatch() ? match.captured(1) : dateValue.left(4));
                        break;
                    }
                }
                std::vector<std::string> publishers;
                for (const auto publisherKey : {"publisher", "journalAbbreviation", "conferenceName",
                                                "proceedingsTitle", "websiteTitle"})
                {
                    if (const auto it = item.meta.find(publisherKey); it != item.meta.end())
                    {
                        publishers.emplace_back(it->second);
                    }
                }
                metaQuery.bindValue(QStringLiteral(":publisher"), QString::fromStdString(join(publishers)));
                metaQuery.bindValue(QStringLiteral(":authors"), QString::fromStdString(join(item.authors)));
                metaQuery.bindValue(QStringLiteral(":tags"), QString::fromStdString(join(item.tags)));
                metaQuery.bindValue(QStringLiteral(":collections"), QString::fromStdString(join(item.collections)));
                metaQuery.bindValue(QStringLiteral(":notes"), QString::fromStdString(join(item.note)));

                if (!metaQuery.exec())
                {
                    qWarning() << "Failed to insert or replace item in Index: " << metaQuery.lastError().text();
                    db.rollback();
                    metaQuery.finish();
                    continue;
                }
                metaQuery.finish();

                QSqlQuery dataQuery(db);
                dataQuery.prepare(IndexSQL::insertOrReplaceData);
                dataQuery.bindValue(QStringLiteral(":id"), item.id);
                json j = item;
                dataQuery.bindValue(QStringLiteral(":obj"), QString::fromStdString(j.dump()));
                if (!dataQuery.exec() || !db.commit())
                {
                    qWarning() << "Failed to insert or replace data in Index: " << dataQuery.lastError().text();
                    db.rollback();
                    dataQuery.finish();
                    continue;

                }
                dataQuery.finish();
                qDebug() << "Inserted item " << item.id << item.key;
            }
            else
            {
                qWarning() << "Failed to start transaction: " << db.lastError().text();
            }
        }

        const auto validIDs = m_zotero.validIDs();
        if (validIDs.empty())
        {
            qWarning() << "[Index] Failed to get valid IDs or Zotero database empty.";
        }
        else
        {
            QSqlQuery deleteQuery(db);
            deleteQuery.prepare(IndexSQL::deleteIdNotIn);
            auto csv = join(validIDs, ',');
            qDebug () << "CSV: " << csv;
            deleteQuery.addBindValue(QString::fromStdString(csv));
            if (!deleteQuery.exec())
            {
                qWarning() << "[Index] Failed to delete invalid IDs: " << deleteQuery.lastError().text();
            } else
            {
                qDebug() << "[Index] Deleted " << deleteQuery.numRowsAffected() << " record(s).";
            }
        }

    }
    QSqlDatabase::removeDatabase(connectionId);
    qDebug() << "[Index] Index successfully updated";
}

std::vector<std::pair<ZoteroItem, float>> Index::search(const QString &needle) const
{
    const auto connectionId = QUuid::createUuid().toString();
    std::vector<std::pair<ZoteroItem, float>> result;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionId);
        db.setDatabaseName(m_dbIndexPath);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (!db.open())
        {
            qWarning() << "[Index] Failed to open Index database: " << db.lastError().text();
            return {};
        }
        QSqlQuery query(db);
        query.prepare(IndexSQL::search);
        query.addBindValue(needle);
        if (!query.exec())
        {
            qWarning() << "[Index] Failed to search Index: " << query.lastError().text();
            return {};
        }

        while (query.next())
        {
            const auto id = query.value(QStringLiteral("rowid")).toInt();
            const auto score = query.value(QStringLiteral("score")).toFloat();
            QSqlQuery dataQuery(db);
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
                const std::string data = dataQuery.value(QStringLiteral("obj")).toString().toStdString();
                const ZoteroItem item = json::parse(data).get<ZoteroItem>();
                result.emplace_back(item, score);
            }
            else
            {
                qWarning() << "Failed to parse item " << id << ": no data";
            }
        }
    }

    QSqlDatabase::removeDatabase(connectionId);
    return result;

}
