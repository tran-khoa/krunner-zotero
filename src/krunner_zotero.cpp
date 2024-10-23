#include "krunner_zotero.h"

#include <KConfigGroup>
#include <QDir>
#include <QStandardPaths>
#include <QString>
#include <index.h>

ZoteroRunner::ZoteroRunner(QObject *parent, const KPluginMetaData &data) : AbstractRunner(parent, data) {}

void ZoteroRunner::init()
{
    reloadConfiguration();

    connect(this, &AbstractRunner::prepare, this,
            [this]()
            {
                m_index = new Index(QDir(m_dbPath).filePath(QStringLiteral("zotero_index.sqlite")),
                                    QDir(m_zoteroPath).filePath(QStringLiteral("zotero.sqlite")));
            });
    connect(this, &AbstractRunner::teardown, this,
            [this]()
            {
                delete m_index;
                qDebug() << "Index destructed";
            });
    this->setMinLetterCount(3);
}

void ZoteroRunner::match(KRunner::RunnerContext &context)
{
    const QString query = context.query();
    QList<KRunner::QueryMatch> matches;
    auto results = m_index->search(query);
    for (const auto &item : results)
    {
        KRunner::QueryMatch match(this);
        match.setText(item.title);
        match.setSubtext(item.authors);
        match.setMultiLine(true);
        match.setIconName(QStringLiteral("zotero"));
        match.setRelevance(item.score / results[0].score);
        match.setCategoryRelevance(KRunner::QueryMatch::CategoryRelevance::High);
        match.setData(item.attachments);
        matches.append(std::move(match));
    }

    for (const auto &match : matches)
    {
        qWarning() << match.text() << match.subtext() << match.relevance();
    }
    context.addMatches(matches);
}

void ZoteroRunner::run(const KRunner::RunnerContext &context, const KRunner::QueryMatch &match)
{
    Q_UNUSED(context);
    qWarning() << match.data().toString();
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

#include "moc_krunner_zotero.cpp"
