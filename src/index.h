#pragma once
#include <zotero.h>

struct IndexEntry
{
    int id;
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
    [[nodiscard]] int length() const;
    [[nodiscard]] std::vector<IndexEntry> search(const QString &needle) const;
    void update(bool force = false) const;


private:
    QString m_dbIndexPath;
    QString m_dbZoteroPath;
    QSqlDatabase m_db;

    void setup();
    bool needs_update() const;
    QDateTime last_modified() const;
};
