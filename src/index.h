#pragma once
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
    Index(const QString&& dbIndexPath, const Zotero&& zotero): m_dbIndexPath(dbIndexPath),
                                                               m_zotero(zotero)
    {
    }

    ~Index() = default;
    [[nodiscard]] std::vector<std::pair<ZoteroItem, float>> search(const QString& needle) const;
    bool setup() const;
    void update(bool force = false) const;

private:
    const QString m_dbIndexPath;
    const Zotero m_zotero;

    [[nodiscard]] bool needs_update() const;
    [[nodiscard]] QDateTime last_modified() const;
};
