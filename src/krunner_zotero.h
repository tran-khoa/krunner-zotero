#pragma once

#include <QLoggingCategory>
#include <KRunner/AbstractRunner>
#include <index.h>

Q_DECLARE_LOGGING_CATEGORY(KRunnerZotero)


class ZoteroRunner final : public KRunner::AbstractRunner
{
    Q_OBJECT

public:
    ZoteroRunner(QObject* parent, const KPluginMetaData& data) : AbstractRunner(parent, data) {}

    void match(KRunner::RunnerContext& context) override;
    void run(const KRunner::RunnerContext& context, const KRunner::QueryMatch& match) override;
    void reloadConfiguration() override;

protected:
    void init() override;

private:
    QString m_zoteroPath;
    QString m_dbPath;
};
