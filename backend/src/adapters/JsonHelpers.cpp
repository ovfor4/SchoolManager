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

Json::Value toJson(const domain::StoredFile& file)
{
    Json::Value value(Json::objectValue);
    value["id"] = file.id;
    value["original_name"] = file.original_name;
    value["stored_name"] = file.stored_name;
    value["mime_type"] = file.mime_type;
    value["status"] = file.status;
    value["size_bytes"] = Json::UInt64(file.size_bytes);
    value["created_at"] = Json::Int64(file.created_at);
    value["updated_at"] = Json::Int64(file.updated_at);
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

Json::Value toJsonArray(const std::vector<domain::Grade>& grades)
{
    Json::Value array(Json::arrayValue);
    for (const auto& grade : grades) {
        array.append(toJson(grade));
    }
    return array;
}

Json::Value toJsonArray(const std::vector<domain::StoredFile>& files)
{
    Json::Value array(Json::arrayValue);
    for (const auto& file : files) {
        array.append(toJson(file));
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
