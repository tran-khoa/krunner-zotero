#include "krunner_zotero.h"

ZoteroRunner::ZoteroRunner(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : AbstractRunner(parent, data) {
    Q_UNUSED(args);
}

void ZoteroRunner::init() {
    reloadConfiguration();
}

void ZoteroRunner::match(KRunner::RunnerContext &context) {
    QString query = context.query();

    QList<KRunner::QueryMatch> matches;
    KRunner::QueryMatch match(this);
    match.setText(query);
    matches.append(match);

    context.addMatches(matches);
}

void ZoteroRunner::run(const KRunner::RunnerContext &context, const KRunner::QueryMatch &match) {
    qWarning() << match.text();
}

void ZoteroRunner::reloadConfiguration() {
}

K_PLUGIN_CLASS_WITH_JSON(ZoteroRunner, "krunner_zotero.json")

#include "krunner_zotero.moc"