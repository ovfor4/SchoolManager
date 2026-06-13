#include "schoolmanager/adapters/JsonHelpers.h"

#include <sstream>
#include <stdexcept>

namespace schoolmanager::adapters {

Json::Value toJson(const domain::StudentSummary& student)
{
    Json::Value value(Json::objectValue);
    value["id"] = student.id;
    value["display_name"] = student.display_name;
    value["folder_path"] = student.folder_path;
    value["created_at"] = Json::Int64(student.created_at);
    value["updated_at"] = Json::Int64(student.updated_at);
    return value;
}

Json::Value toJson(const domain::StudentInfoDefinition& field)
{
    Json::Value value(Json::objectValue);
    value["id"] = field.id;
    value["name"] = field.name;
    value["display_name"] = field.display_name;
    value["value_type"] = field.value_type;
    value["created_at"] = Json::Int64(field.created_at);
    value["updated_at"] = Json::Int64(field.updated_at);
    return value;
}

Json::Value toJson(const domain::StudentInfoField& field)
{
    Json::Value value(Json::objectValue);
    value["id"] = field.id;
    value["name"] = field.name;
    value["display_name"] = field.display_name;
    value["value_type"] = field.value_type;
    value["value"] = field.value;
    value["updated_at"] = Json::Int64(field.updated_at);
    return value;
}

Json::Value toJson(const domain::Grade& grade)
{
    Json::Value value(Json::objectValue);
    value["id"] = grade.id;
    value["title"] = grade.title;
    value["score"] = grade.score;
    value["max_score"] = grade.max_score;
    value["occurred_on"] = grade.occurred_on;
    value["notes"] = grade.notes;
    value["updated_at"] = Json::Int64(grade.updated_at);
    return value;
}

Json::Value toJson(const domain::FileEntry& entry)
{
    Json::Value value(Json::objectValue);
    value["id"] = entry.id;
    value["context_type"] = entry.context_type;
    value["context_id"] = entry.context_id;
    if (entry.parent_id) {
        value["parent_id"] = *entry.parent_id;
    } else {
        value["parent_id"] = Json::nullValue;
    }
    value["kind"] = entry.kind;
    value["name"] = entry.name;
    value["mime_type"] = entry.mime_type;
    value["size_bytes"] = Json::UInt64(entry.size_bytes);
    value["status"] = entry.status;
    value["created_at"] = Json::Int64(entry.created_at);
    value["updated_at"] = Json::Int64(entry.updated_at);
    if (entry.trashed_at) {
        value["trashed_at"] = Json::Int64(*entry.trashed_at);
    } else {
        value["trashed_at"] = Json::nullValue;
    }
    return value;
}

Json::Value toJson(const domain::TrashEntry& entry)
{
    Json::Value value(Json::objectValue);
    value["id"] = entry.id;
    value["context_type"] = entry.context_type;
    value["context_id"] = entry.context_id;
    value["root_entry_id"] = entry.root_entry_id;
    if (entry.original_parent_id) {
        value["original_parent_id"] = *entry.original_parent_id;
    } else {
        value["original_parent_id"] = Json::nullValue;
    }
    value["root_name"] = entry.root_name;
    value["root_kind"] = entry.root_kind;
    value["item_count"] = Json::UInt64(entry.item_count);
    value["trashed_at"] = Json::Int64(entry.trashed_at);
    return value;
}

Json::Value toJsonArray(const std::vector<domain::StudentSummary>& students)
{
    Json::Value array(Json::arrayValue);
    for (const auto& student : students) {
        array.append(toJson(student));
    }
    return array;
}

Json::Value toJsonArray(const std::vector<domain::StudentInfoDefinition>& fields)
{
    Json::Value array(Json::arrayValue);
    for (const auto& field : fields) {
        array.append(toJson(field));
    }
    return array;
}

Json::Value toJsonArray(const std::vector<domain::StudentInfoField>& fields)
{
    Json::Value array(Json::arrayValue);
    for (const auto& field : fields) {
        array.append(toJson(field));
    }
    return array;
}

Json::Value toJsonArray(const std::vector<domain::Grade>& grades)
{
    Json::Value array(Json::arrayValue);
    for (const auto& grade : grades) {
        array.append(toJson(grade));
    }
    return array;
}

Json::Value toJsonArray(const std::vector<domain::FileEntry>& entries)
{
    Json::Value array(Json::arrayValue);
    for (const auto& entry : entries) {
        array.append(toJson(entry));
    }
    return array;
}

Json::Value toJsonArray(const std::vector<domain::TrashEntry>& entries)
{
    Json::Value array(Json::arrayValue);
    for (const auto& entry : entries) {
        array.append(toJson(entry));
    }
    return array;
}

std::string writeJsonCompact(const Json::Value& value)
{
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, value);
}

std::string scalarToString(const Json::Value& value)
{
    if (value.isNull()) {
        return {};
    }
    if (value.isString()) {
        return value.asString();
    }
    if (value.isBool()) {
        return value.asBool() ? "true" : "false";
    }
    if (value.isInt64()) {
        return std::to_string(value.asInt64());
    }
    if (value.isUInt64()) {
        return std::to_string(value.asUInt64());
    }
    if (value.isDouble()) {
        std::ostringstream out;
        out << value.asDouble();
        return out.str();
    }
    throw std::runtime_error("expected scalar JSON value");
}

}  // namespace schoolmanager::adapters
