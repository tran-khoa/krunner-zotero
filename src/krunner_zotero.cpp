#include "krunner_zotero.h"

#include <KConfigGroup>
#include <KIO/OpenUrlJob>
#include <QDir>
#include <QStandardPaths>
#include <QString>
#include <index.h>

Q_LOGGING_CATEGORY(KRunnerZotero, "krunner-zotero")


void ZoteroRunner::init()
{
    reloadConfiguration();
    this->setMinLetterCount(3);
    const Zotero zotero(m_zoteroPath);
    const Index index(m_dbPath, zotero);
    // ReSharper disable once CppExpressionWithoutSideEffects
    index.setup();

    connect(this, &AbstractRunner::prepare, this,
            [this]() { Index(m_dbPath, Zotero(m_zoteroPath)).update(); });
}

void ZoteroRunner::match(KRunner::RunnerContext &context)
{
    QList<KRunner::QueryMatch> matches;
    const Zotero zotero(m_zoteroPath);
    const Index index(m_dbPath, zotero);
    const auto results = index.search(context.query());
    for (const auto &[item, score] : results)
    {
        KRunner::QueryMatch match(this);
        match.setText(
            QStringLiteral("<b>%1</b><br><i>%2 (%3)</i>").arg(QString::fromStdString(item.meta.at("title")),
                                                         item.authorSummary(), item.year()));
        match.setData(QString::fromStdString(json(item).dump()));
        match.setMultiLine(true);
        match.setIconName(QStringLiteral("zotero"));
        match.setRelevance(score / results[0].second);
        match.setCategoryRelevance(KRunner::QueryMatch::CategoryRelevance::High);
        matches.emplace_back(match);
    }
    context.addMatches(matches);
}

void ZoteroRunner::run(const KRunner::RunnerContext &context, const KRunner::QueryMatch &match)
{
    Q_UNUSED(context);
    const ZoteroItem item = json::parse(match.data().toString().toStdString()).get<ZoteroItem>();
    for (const auto &attachment : item.attachments)
    {
        if (attachment.contentType == "application/pdf")
        {
            const QUrl url(QStringLiteral("zotero://open-pdf/library/items/") +
                QString::fromStdString(attachment.key));
            // ReSharper disable once CppDFAMemoryLeak
            const auto job = new KIO::OpenUrlJob(url);
            job->start();
            return;
        }
    }
    qCDebug(KRunnerZotero) << "No PDF attachment found, opening Zotero item." << QString::fromStdString(item.key);
    const QUrl url(QStringLiteral("zotero://select/library/items/") + QString::fromStdString(item.key));
    // ReSharper disable once CppDFAMemoryLeak
    const auto job = new KIO::OpenUrlJob(url);
    job->start();
}

void ZoteroRunner::reloadConfiguration()
{
    const KConfigGroup c = config();
    m_zoteroPath = c.readEntry("zoteroPath", QDir::home().filePath(QStringLiteral("Zotero/zotero.sqlite")));
    const QDir KRunnerPath = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first();
    if (!KRunnerPath.exists())
        if (!KRunnerPath.mkpath(QStringLiteral(".")))
            qCDebug(KRunnerZotero) << "Failed to create KRunner directory.";
    m_dbPath = c.readEntry("dbPath", KRunnerPath.filePath(QStringLiteral("zotero.sqlite")));
}


K_PLUGIN_CLASS_WITH_JSON(ZoteroRunner, "krunner_zotero.json")

#include "krunner_zotero.moc"
#include "moc_krunner_zotero.cpp"
