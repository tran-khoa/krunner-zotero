#pragma once
#include <QMutex>
#include <zotero.h>

struct IndexEntry
{
    int id;
    QString key;
    QString title;
    QString year;
    QString creators;
    QString authors;
    QString editors;
    QString tags;
    QString collections;
    QString attachments;
    QString notes;
    QString abstract;
    float score;
};

class Index
{
public:
    Index(const QString &dbIndexPath, const QString &dbZoteroPath);
    ~Index();
    [[nodiscard]] std::vector<std::pair<ZoteroItem, float>> search(const QString &needle) const;
    void update(bool force = false) const;


private:
    QString m_dbIndexPath;
    QString m_dbZoteroPath;
    QSqlDatabase m_db;
    QString m_indexConnectionId;
    QMutex m_mutex;

    void setup();
    bool needs_update() const;
    [[nodiscard]] QDateTime last_modified() const;
};
