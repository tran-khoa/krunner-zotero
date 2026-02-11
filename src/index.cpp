#include "index.h"

#include <QFileInfo>

#include "zotero.h"

#include <QString>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>


Q_LOGGING_CATEGORY(KRunnerZoteroIndex, "krunner-zotero/index")


using json = nlohmann::json;

constexpr int DB_VERSION = 1;

template <typename T>
std::string join(const std::vector<T>& vec, const char sep = ' ')
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
const std::array createTables = {QStringLiteral(R"(
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
            key TEXT PRIMARY KEY NOT NULL,
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
const std::array reset = {QStringLiteral("DROP TABLE IF EXISTS `data`;"),
                          QStringLiteral("DROP TABLE IF EXISTS `dbinfo`;"),
                          QStringLiteral("DROP TABLE IF EXISTS `search`;"),
                          QStringLiteral("VACUUM;"),
                          QStringLiteral("PRAGMA INTEGRITY_CHECK;")};
const auto insertOrReplaceSearch = QStringLiteral(
    "INSERT OR REPLACE "
    "INTO search (rowid, key, title, shortTitle, doi, year, authors, tags, collections, notes, abstract, publisher) "
    "VALUES(:rowid, :key, :title, :shortTitle, :doi, :year, :authors, :tags, :collections, :notes, :abstract, :publisher);");
const auto insertOrReplaceData = QStringLiteral("INSERT OR REPLACE INTO data (key, obj) VALUES(:key, :obj);");
const auto search = QStringLiteral(
    "SELECT rowid, *, bm25(search, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.7, 0.5, 0.4, 0.4, 0.4) "
    "AS score FROM search WHERE search MATCH ? "
    "ORDER BY score LIMIT 10");
const auto selectData = QStringLiteral("SELECT obj FROM data WHERE key = ?");
const auto deleteKeysNotInSearch = QStringLiteral("DELETE FROM search WHERE key NOT IN (%1);");
const auto deleteKeysNotInData = QStringLiteral("DELETE FROM data WHERE key NOT IN (%1);");

} // namespace IndexSQL


bool Index::setup() const
{
    /**
    * @brief Setup the index database
    *
    * @return true if the database was (re-)created, false otherwise
    */
    qCDebug(KRunnerZoteroIndex()) << "Setting up index...";
    bool do_update = false;
    const auto connectionId = QUuid::createUuid().toString();
    {
        auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionId);
        db.setDatabaseName(m_dbIndexPath);
        if (!db.open())
        {
            qCCritical(KRunnerZoteroIndex) << "Failed to open Index database: " << db.lastError().text();
            return false;
        }

        bool do_create_tables = true;

        QSqlQuery versionQuery(db);
        versionQuery.exec(IndexSQL::getVersion);
        if (versionQuery.next())
        {
            if (versionQuery.value(QStringLiteral("version")).toInt() != DB_VERSION)
            {
                qCInfo(KRunnerZoteroIndex) << "Database version outdated, most recent version is: " << DB_VERSION <<
 " but is "
                    << versionQuery.value(QStringLiteral("version")).toInt();

                if (db.transaction())
                {
                    QSqlQuery resetQuery(db);
                    for (const QString& statement : IndexSQL::reset)
                    {
                        resetQuery.exec(statement);
                    }
                    if (!db.commit())
                    {
                        qCCritical(KRunnerZoteroIndex) << "Failed to commit reset" << db.lastError().text();
                        db.rollback();
                        do_create_tables = false;
                    }
                    else
                    {
                        qCInfo(KRunnerZoteroIndex) << "Reset completed";
                    }
                }
                else
                {
                    qCCritical(KRunnerZoteroIndex) << "Failed to start transaction" << db.lastError().text();
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
            qCInfo(KRunnerZoteroIndex) << "Creating tables...";
            if (db.transaction())
            {
                QSqlQuery createQuery(db);
                for (const QString& statement : IndexSQL::createTables)
                {
                    createQuery.exec(statement);
                }
                if (!db.commit())
                {
                    qCCritical(KRunnerZoteroIndex) << "Failed to commit create tables" << db.lastError().text();
                    db.rollback();
                }
                else
                {
                    qCInfo(KRunnerZoteroIndex) << "Tables created";
                    do_update = true;
                }
            }
            else
            {
                qCCritical(KRunnerZoteroIndex) << "Failed to start transaction" << db.lastError().text();
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

QVariant getOrNull(const std::unordered_map<std::string, std::string>& m, const std::string& key)
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
        qCDebug(KRunnerZoteroIndex) << "Index is up to date.";
        return;
    }

    qCInfo(KRunnerZoteroIndex) << "Updating index...";
    qCDebug(KRunnerZoteroIndex()) << "Last update of index: " << last_modified().toString();
    qCDebug(KRunnerZoteroIndex()) << "Last update of Zotero: " << m_zotero.lastModified().toString();
    const auto connectionId = QUuid::createUuid().toString();
    {
        auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionId);
        db.setDatabaseName(m_dbIndexPath);
        if (!db.open())
        {
            qCCritical(KRunnerZoteroIndex) << "Failed to open Index database: " << db.lastError().text();
            return;
        }

        QDateTime last_modified_dt;
        if (force) {
            // earliest date possible, so all items are returned
            last_modified_dt = QDateTime(QDate(1970, 1, 1), QTime(0, 0));
        } else {
            last_modified_dt = last_modified();
        }

        for (const ZoteroItem &&item : m_zotero.items(last_modified_dt)) {
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
                metaQuery.bindValue(QStringLiteral(":year"), item.year());
                std::vector<std::string> publishers;
                for (const auto publisherKey : {
                         "publisher", "journalAbbreviation", "conferenceName",
                         "proceedingsTitle", "websiteTitle"
                     })
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
                    qCCritical(KRunnerZoteroIndex) << "Failed to insert or replace item in Index (meta): " << metaQuery.lastError().text();
                    db.rollback();
                    metaQuery.finish();
                    continue;
                }
                metaQuery.finish();

                QSqlQuery dataQuery(db);
                dataQuery.prepare(IndexSQL::insertOrReplaceData);
                dataQuery.bindValue(QStringLiteral(":key"), QString::fromStdString(item.key));
                json j = item;
                dataQuery.bindValue(QStringLiteral(":obj"), QString::fromStdString(j.dump()));
                if (!dataQuery.exec() || !db.commit())
                {
                    qCCritical(KRunnerZoteroIndex) << "Failed to insert or replace data in Index (data): " << dataQuery.lastError().text();
                    db.rollback();
                    dataQuery.finish();
                    continue;
                }
                dataQuery.finish();
                qCDebug(KRunnerZoteroIndex) << "Inserted item " << item.id << item.key;
            }
            else
            {
                qCCritical(KRunnerZoteroIndex) << "Failed to start transaction: " << db.lastError().text();
            }
        }

        if (const auto validKeys = m_zotero.validKeys(); validKeys.empty()) {
            qCWarning(KRunnerZoteroIndex) << "Failed to get valid IDs or Zotero database empty.";
        } else {
            // add double quotes around each key
            std::vector<std::string> quotedKeys;
            quotedKeys.reserve(validKeys.size());
            for (const auto &key : validKeys) {
                quotedKeys.push_back('"' + key + '"');
            }

            if (QSqlQuery deleteQuerySearch(db); !deleteQuerySearch.exec(IndexSQL::deleteKeysNotInSearch.arg(QString::fromStdString(join(quotedKeys, ','))))) {
                qCCritical(KRunnerZoteroIndex) << "Failed to delete invalid IDs: " << deleteQuerySearch.lastError().text();
            } else {
                qCDebug(KRunnerZoteroIndex) << "Deleted " << deleteQuerySearch.numRowsAffected() << " record(s) from search table.";
            }
            if (QSqlQuery deleteQueryData(db); !deleteQueryData.exec(IndexSQL::deleteKeysNotInData.arg(QString::fromStdString(join(quotedKeys, ','))))) {
                qCCritical(KRunnerZoteroIndex) << "Failed to delete invalid IDs: " << deleteQueryData.lastError().text();
            } else {
                qCDebug(KRunnerZoteroIndex) << "Deleted " << deleteQueryData.numRowsAffected() << " record(s) from data table.";
            }
        }
    }
    QSqlDatabase::removeDatabase(connectionId);
    qCDebug(KRunnerZoteroIndex) << "Index successfully updated";
}

std::vector<std::pair<ZoteroItem, float>> Index::search(QString&& needle) const
{
    const auto connectionId = QUuid::createUuid().toString();

    // escape all double quotes in needle
    needle.replace(QStringLiteral("\""), QStringLiteral("\"\""));
    needle.prepend(QStringLiteral("\""));
    needle.append(QStringLiteral("\""));
    std::vector<std::pair<ZoteroItem, float>> result;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionId);
        db.setDatabaseName(m_dbIndexPath);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (!db.open())
        {
            qCCritical(KRunnerZoteroIndex) << "Failed to open Index database: " << db.lastError().text();
            return {};
        }
        QSqlQuery query(db);
        query.prepare(IndexSQL::search);
        query.addBindValue(needle);
        if (!query.exec())
        {
            qCCritical(KRunnerZoteroIndex) << "Failed to search Index: " << query.lastError().text();
            qCCritical(KRunnerZoteroIndex) << "with query" << query.lastQuery();
            return {};
        }

        while (query.next())
        {
            const auto key = query.value(QStringLiteral("key")).toString();
            const auto score = query.value(QStringLiteral("score")).toFloat();
            QSqlQuery dataQuery(db);
            dataQuery.prepare(IndexSQL::selectData);
            dataQuery.addBindValue(key);
            dataQuery.exec();
            if (!dataQuery.exec())
            {
                qCCritical(KRunnerZoteroIndex) << "Failed to get data for item " << key << ": " << dataQuery.lastError().text();
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
                qCDebug(KRunnerZoteroIndex) << "Failed to get data for item " << key << ": no data";
            }
        }
    }

    QSqlDatabase::removeDatabase(connectionId);
    return result;
}
