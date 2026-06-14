#pragma once

#include "schoolmanager/application/ApplicationError.h"
#include "schoolmanager/infra/FileManagerService.h"

#include <drogon/HttpResponse.h>
#include <json/json.h>

#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace schoolmanager::adapters {

drogon::HttpResponsePtr jsonResponse(Json::Value body,
                                     drogon::HttpStatusCode status = drogon::k200OK);
drogon::HttpResponsePtr errorResponse(const std::string& message,
                                      drogon::HttpStatusCode status);

drogon::HttpStatusCode statusForApplicationError(application::ApplicationErrorCode code);
drogon::HttpStatusCode statusForFileManagerError(infra::FileManagerErrorCode code);

std::string bodyString(const Json::Value& body,
                       std::string_view field,
                       std::string_view fallback = "");
std::map<std::string, std::string> collectStringFields(
    const Json::Value& body,
    std::initializer_list<std::string_view> allowedFields);
Json::Value changedFieldsJson(const std::map<std::string, std::string>& changes);

std::optional<std::string> optionalNonEmpty(std::string value);

}  // namespace schoolmanager::adapters
