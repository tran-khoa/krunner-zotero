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
    Index(const QString &m_dbPath, const Zotero &zotero);
    ~Index();
    [[nodiscard]] std::optional<QDateTime> lastModified() const;
    [[nodiscard]] int length() const;
    [[nodiscard]] std::vector<IndexEntry> search(const QString &needle) const;
    void update(bool force = false) const;


private:
    QString m_dbPath;
    Zotero m_zotero;
    QSqlDatabase m_db;

    void setup();
};
