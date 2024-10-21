#include "krunner_zotero.h"

#include <KConfigGroup>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRelationalTableModel>
#include <QStandardPaths>
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
                m_index = new Index(QDir(m_dbPath).filePath(QStringLiteral("zotero_index.sqlite")),
                                    QDir(m_zoteroPath).filePath(QStringLiteral("zotero.sqlite")));
            });
    connect(this, &AbstractRunner::teardown, this, [this]() { delete m_index; });
}

void ZoteroRunner::match(KRunner::RunnerContext &context)
{
    const QString query = context.query();
    if (query.length() < 3)
    {
        return;
    }

    QList<KRunner::QueryMatch> matches;
    auto results = m_index->search(query);
    for (const auto &item : results)
    {
        KRunner::QueryMatch match(this);
        match.setText(item.title);
        match.setSubtext(item.abstract);
        match.setIconName(QStringLiteral("zotero"));
        match.setRelevance(item.score / results[0].score);
        matches.append(match);
    }

    context.addMatches(matches);
}

void ZoteroRunner::run(const KRunner::RunnerContext &context, const KRunner::QueryMatch &match)
{
    qWarning() << match.text();
}

void ZoteroRunner::reloadConfiguration()
{
    const KConfigGroup c = config();
    m_zoteroPath = c.readEntry("zoteroPath", QDir::home().filePath(QStringLiteral("Zotero")));
    m_dbPath = c.readEntry("dbPath", QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first());
    qDebug() << "dbPath: " << m_dbPath;
}

K_PLUGIN_CLASS_WITH_JSON(ZoteroRunner, "krunner_zotero.json")

#include "krunner_zotero.moc"
