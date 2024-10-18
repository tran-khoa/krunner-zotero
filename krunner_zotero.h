#pragma once

#include <KRunner/AbstractRunner>

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
  QString m_path;
  QString m_triggerWord;
};
