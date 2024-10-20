#include "krunner_zotero.h"

#include <KConfigGroup>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRelationalTableModel>
#include <QString>
#include <index.h>

ZoteroRunner::ZoteroRunner(QObject *parent, const KPluginMetaData &data, const QVariantList &args) :
    AbstractRunner(parent, data)
{
    Q_UNUSED(args);
}

void ZoteroRunner::init()
{
    reloadConfiguration();

    connect(this, &AbstractRunner::prepare, this,
            [this]()
            {
                m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
                m_db.setDatabaseName(QDir(m_zoteroPath).filePath(QStringLiteral("zotero.sqlite")));
                m_db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
                if (!m_db.open())
                {
                    qWarning() << "Failed to open Zotero database";
                }
            });

    connect(this, &AbstractRunner::teardown, this, [this]() { m_db.close(); });
}

void ZoteroRunner::match(KRunner::RunnerContext &context)
{
    const QString query = context.query();

    QSqlQuery sqliteQuery;

    const QString sqliteQueryString = QStringLiteral(R"(
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
    -- Ignore notes and attachments
    WHERE items.itemTypeID not IN (1, 14)
    AND deletedItems.dateDeleted IS NULL
    )");
    sqliteQuery.exec(sqliteQueryString);
    while (sqliteQuery.next())
    {
        KRunner::QueryMatch match(this);
        match.setText(sqliteQuery.value(2).toString());
        context.addMatch(match);
    }
}

void ZoteroRunner::run(const KRunner::RunnerContext &context, const KRunner::QueryMatch &match)
{
    qWarning() << match.text();
}

void ZoteroRunner::reloadConfiguration()
{
    const KConfigGroup c = config();
    m_zoteroPath = c.readEntry("zoteroPath", QDir::home().filePath(QStringLiteral("Zotero")));
}

K_PLUGIN_CLASS_WITH_JSON(ZoteroRunner, "krunner_zotero.json")

#include "krunner_zotero.moc"
