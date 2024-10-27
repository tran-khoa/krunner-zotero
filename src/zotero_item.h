#pragma once

#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace nlohmann::literals;




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
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ZoteroItem, id, key, modified, meta, attachments, collections, note, tags, authors)