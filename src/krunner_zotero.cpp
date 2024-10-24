#include "krunner_zotero.h"

#include <KConfigGroup>
#include <KIO/OpenUrlJob>
#include <QDir>
#include <QStandardPaths>
#include <QString>
#include <index.h>
#include <zotero.pb.h>

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
        match.setText(QString::fromStdString(item.first.title()));
        match.setSubtext(QString::fromStdString(item.first.date()));
        match.setMultiLine(true);
        match.setIconName(QStringLiteral("zotero"));
        match.setRelevance(item.second / results[0].second);
        match.setCategoryRelevance(KRunner::QueryMatch::CategoryRelevance::High);

        std::string serializedItem;
        if (!item.first.SerializeToString(&serializedItem))
        {
            qDebug() << "Failed to serialize to data.";
            return;
        }
        match.setData(QByteArray(serializedItem.data(), static_cast<int>(serializedItem.size())));
        matches.append(std::move(match));
    }
    context.addMatches(matches);
}

void ZoteroRunner::run(const KRunner::RunnerContext &context, const KRunner::QueryMatch &match)
{
    Q_UNUSED(context);
    const auto payload = match.data().toByteArray();
    ZoteroItem item;
    if (!item.ParseFromArray(payload.data(), static_cast<int>(payload.size())))
    {
        qWarning() << "Failed to parse data item.";
    }

    for (const auto &attachment : item.attachments())
    {
        if (attachment.contenttype() == QStringLiteral("application/pdf"))
        {
            const QUrl url(QStringLiteral("zotero://open-pdf/library/items/") +
                           QString::fromStdString(attachment.key()));
            // ReSharper disable once CppDFAMemoryLeak
            const auto job = new KIO::OpenUrlJob(url);
            job->start();
            return;
        }
    }
    qDebug() << "No PDF attachment found, opening Zotero item." << QString::fromStdString(item.key());
    const QUrl url(QStringLiteral("zotero://select/library/items/") + QString::fromStdString(item.key()));
    // ReSharper disable once CppDFAMemoryLeak
    const auto job = new KIO::OpenUrlJob(url);
    job->start();
}

void ZoteroRunner::reloadConfiguration()
{
    const KConfigGroup c = config();
    m_zoteroPath = c.readEntry("zoteroPath", QDir::home().filePath(QStringLiteral("Zotero")));
    m_dbPath = c.readEntry("dbPath", QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first());
}


K_PLUGIN_CLASS_WITH_JSON(ZoteroRunner, "krunner_zotero.json")

#include "krunner_zotero.moc"

#include "moc_krunner_zotero.cpp"
