#pragma once
#include <QString>
#include <functional>
#include <numeric>
#include <qcontainerfwd.h>


template <class ItemType>
QVector<ItemType> vectorFilter(const QVector<ItemType> &items, const std::function<bool(const ItemType &)> &predicate)
{
    QVector<ItemType> result;
    std::copy_if(items.cbegin(), items.cend(), std::back_inserter(result), predicate);
    return result;
}


template <class ItemType>
QString vectorMapConcat(const QVector<ItemType> &items, const std::function<QString(const ItemType &)> &mapper,
                        const QString &separator = QStringLiteral(" "))
{
    const auto totalSize = std::accumulate(items.cbegin(), items.cend(), 0, [&mapper](const auto &acc, const auto &item)
                                           { return acc + mapper(item).size(); }) +
        (items.size() - 1) * separator.size();
    QString result;
    result.reserve(totalSize);

    for (auto it = items.cbegin(); it != items.cend(); ++it)
    {
        if (it != items.cbegin())
        {
            result.append(separator);
        }
        result.append(mapper(*it));
    }
    return result;
}
