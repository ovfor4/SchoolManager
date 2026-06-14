#include "schoolmanager/adapters/HttpJson.h"

#include "schoolmanager/adapters/JsonHelpers.h"

#include <utility>

namespace schoolmanager::adapters {

drogon::HttpResponsePtr jsonResponse(Json::Value body, drogon::HttpStatusCode status)
{
    auto response = drogon::HttpResponse::newHttpJsonResponse(std::move(body));
    response->setStatusCode(status);
    return response;
}

drogon::HttpResponsePtr errorResponse(const std::string& message, drogon::HttpStatusCode status)
{
    Json::Value body(Json::objectValue);
    body["error"] = message;
    return jsonResponse(std::move(body), status);
}

drogon::HttpStatusCode statusForApplicationError(application::ApplicationErrorCode code)
{
    switch (code) {
    case application::ApplicationErrorCode::BadRequest:
        return drogon::k400BadRequest;
    case application::ApplicationErrorCode::NotFound:
        return drogon::k404NotFound;
    }
    return drogon::k500InternalServerError;
}

drogon::HttpStatusCode statusForFileManagerError(infra::FileManagerErrorCode code)
{
    switch (code) {
    case infra::FileManagerErrorCode::BadRequest:
        return drogon::k400BadRequest;
    case infra::FileManagerErrorCode::NotFound:
        return drogon::k404NotFound;
    case infra::FileManagerErrorCode::Conflict:
        return drogon::k409Conflict;
    }
    return drogon::k500InternalServerError;
}

std::string bodyString(const Json::Value& body, std::string_view field, std::string_view fallback)
{
    const std::string key(field);
    if (!body.isMember(key)) {
        return std::string(fallback);
    }
    return scalarToString(body[key]);
}

std::map<std::string, std::string> collectStringFields(
    const Json::Value& body,
    std::initializer_list<std::string_view> allowedFields)
{
    std::map<std::string, std::string> changes;
    for (const auto field : allowedFields) {
        const std::string key(field);
        if (body.isMember(key)) {
            changes.emplace(key, scalarToString(body[key]));
        }
    }
    return changes;
}

Json::Value changedFieldsJson(const std::map<std::string, std::string>& changes)
{
    Json::Value fields(Json::arrayValue);
    for (const auto& [field, _] : changes) {
        fields.append(field);
    }
    return fields;
}

std::optional<std::string> optionalNonEmpty(std::string value)
{
    if (value.empty()) {
        return std::nullopt;
    }
    return value;
}

}  // namespace schoolmanager::adapters
