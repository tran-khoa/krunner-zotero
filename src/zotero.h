#pragma once

#include <QDateTime>
#include <QString>
#include <generator>
#include <utility>
#include "zotero_item.h"

class Zotero
{
public:
    explicit Zotero(QString dbPath) : m_dbPath(std::move(dbPath)) {}
    ~Zotero() = default;
    [[nodiscard]] QDateTime lastModified() const;
    [[nodiscard]] std::generator<const ZoteroItem&&>
    items(const std::optional<const QDateTime> &lastModified = std::nullopt) const;
    [[nodiscard]] std::vector<int> validIDs() const;


private:
    const QString m_dbPath;
};
