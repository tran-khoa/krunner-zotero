#pragma once

#include <KRunner/AbstractRunner>
#include <QSqlDatabase>
#include <index.h>

class ZoteroRunner : public KRunner::AbstractRunner
{
    Q_OBJECT

public:
    ZoteroRunner(QObject *parent, const KPluginMetaData &data, const QVariantList &args);

    void match(KRunner::RunnerContext &context) override;
    void run(const KRunner::RunnerContext &context, const KRunner::QueryMatch &match) override;
    void reloadConfiguration() override;

protected:
    void init() override;

private:
    QString m_zoteroPath;
    QString m_dbPath;
    Index *m_index;
};
