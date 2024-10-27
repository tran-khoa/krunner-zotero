#pragma once

#include <QRegularExpression>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace nlohmann::literals;


const QRegularExpression ZOTERO_DATE_REGEX(QStringLiteral(R"((\d{4})-(\d{2})-(\d{2}).*)"));


struct Attachment
{
    std::string key;
    std::string path; // storage:Mirzadeh2022ArchitectureMattersContinualLearning.pdf
    std::string title; // Preprint PDF
    std::string url; // http://arxiv.org/pdf/2202.00275v1
    std::string contentType; // application/pdf
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Attachment, key, path, title, url, contentType)


struct ZoteroItem
{
    int id;
    std::string key; // TP6IKMQ6
    std::string modified;
    std::unordered_map<std::string, std::string> meta;

    std::vector<Attachment> attachments;
    std::vector<std::string> collections;
    std::vector<std::string> note;
    std::vector<std::string> tags;
    std::vector<std::string> authors;

    [[nodiscard]] QDateTime modifiedDateTime() const
    {
        return QDateTime::fromString(QString::fromStdString(modified), QStringLiteral("yyyy-MM-dd hh:mm:ss"));
    }

    QString authorSummary() const
    {
        if (authors.empty())
        {
            return {};
        }
        if (authors.size() == 1)
        {
            return QString::fromStdString(authors.front());
        }
        if (authors.size() == 2)
        {
            return QString::fromStdString(authors.front()) + QStringLiteral(" and ") + QString::fromStdString(authors.back());
        }
        return QString::fromStdString(authors.front()) + QStringLiteral(" et al.");
    }

    QString year() const
    {
        for (const auto dateKey : {"dateEnacted", "dateDecided", "filingDate", "issueDate", "date"})
        {
            if (const auto it = meta.find(dateKey); it != meta.end())
            {
                const auto dateValue = QString::fromStdString(it->second);
                const QRegularExpressionMatch match = ZOTERO_DATE_REGEX.match(dateValue);
                return match.hasMatch() ? match.captured(1) : dateValue.left(4);
            }
        }
        return {};
    }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ZoteroItem, id, key, modified, meta, attachments, collections, note, tags, authors)