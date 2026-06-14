#include "schoolmanager/adapters/FileTemplateController.h"

#include "schoolmanager/adapters/HttpJson.h"
#include "schoolmanager/adapters/JsonHelpers.h"

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace schoolmanager::adapters {

namespace {

using Callback = std::function<void(const drogon::HttpResponsePtr&)>;

application::FileTemplateGenerateRequest parseGenerateRequest(const Json::Value& body)
{
    if (!body.isObject() || !body.isMember("template_entry_id")) {
        throw application::ApplicationError(
            application::ApplicationErrorCode::BadRequest,
            "template_entry_id is required");
    }

    application::FileTemplateGenerateRequest request;
    request.template_entry_id = scalarToString(body["template_entry_id"]);
    if (body.isMember("student_ids")) {
        if (!body["student_ids"].isArray()) {
            throw application::ApplicationError(
                application::ApplicationErrorCode::BadRequest,
                "student_ids must be an array of strings");
        }
        for (const auto& studentId : body["student_ids"]) {
            if (!studentId.isString()) {
                throw application::ApplicationError(
                    application::ApplicationErrorCode::BadRequest,
                    "student_ids must be an array of strings");
            }
            request.student_ids.push_back(studentId.asString());
        }
    }

    const auto optionalString = [&body](std::string_view field) -> std::optional<std::string> {
        const auto key = std::string(field);
        if (!body.isMember(key) || body[key].isNull()) {
            return std::nullopt;
        }
        if (!body[key].isString()) {
            throw application::ApplicationError(
                application::ApplicationErrorCode::BadRequest,
                key + " must be a string");
        }
        return body[key].asString();
    };
    request.source_format = optionalString("source_format");
    request.export_format = optionalString("export_format");
    return request;
}

Json::Value stringArrayJson(const std::vector<std::string>& values)
{
    Json::Value array(Json::arrayValue);
    for (const auto& value : values) {
        array.append(value);
    }
    return array;
}

Json::Value capabilitiesJson(const application::DocumentFormatCapabilities& capabilities)
{
    Json::Value body(Json::objectValue);
    body["default_source_format"] = capabilities.default_source_format;
    body["source_formats"] = Json::Value(Json::arrayValue);
    for (const auto& format : capabilities.source_formats) {
        Json::Value item(Json::objectValue);
        item["id"] = format.id;
        item["label"] = format.label;
        item["extensions"] = stringArrayJson(format.extensions);
        item["mime_types"] = stringArrayJson(format.mime_types);
        item["default_export_format"] = format.default_export_format;
        item["export_formats"] = stringArrayJson(format.export_format_ids);
        item["supports_student_variables"] = format.supports_student_variables;
        body["source_formats"].append(std::move(item));
    }

    body["export_formats"] = Json::Value(Json::arrayValue);
    for (const auto& format : capabilities.export_formats) {
        Json::Value item(Json::objectValue);
        item["id"] = format.id;
        item["label"] = format.label;
        item["extension"] = format.extension;
        item["mime_type"] = format.mime_type;
        body["export_formats"].append(std::move(item));
    }
    return body;
}

std::string attachmentDisposition(const std::string& fileName)
{
    return "attachment; filename=\"" + fileName + "\"";
}

}  // namespace

FileTemplateController::FileTemplateController(
    std::shared_ptr<application::FileTemplateUseCases> fileTemplates)
    : file_templates_(std::move(fileTemplates))
{
}

void FileTemplateController::capabilities(const drogon::HttpRequestPtr&,
                                          Callback&& callback)
{
    try {
        callback(jsonResponse(capabilitiesJson(file_templates_->capabilities()), drogon::k200OK));
    } catch (const application::ApplicationError& error) {
        callback(errorResponse(error.what(), statusForApplicationError(error.code())));
    } catch (const std::exception& error) {
        callback(errorResponse(error.what(), drogon::k500InternalServerError));
    }
}

void FileTemplateController::generate(const drogon::HttpRequestPtr& request,
                                      Callback&& callback)
{
    try {
        const auto json = request->getJsonObject();
        if (!json) {
            callback(errorResponse("JSON body is required", drogon::k400BadRequest));
            return;
        }

        auto download = file_templates_->generate(parseGenerateRequest(*json));

        auto response = drogon::HttpResponse::newHttpResponse();
        response->setStatusCode(drogon::k200OK);
        response->setContentTypeString(download.mime_type);
        response->addHeader("Content-Disposition", attachmentDisposition(download.file_name));
        response->setBody(std::move(download.content));
        callback(response);
    } catch (const application::ApplicationError& error) {
        callback(errorResponse(error.what(), statusForApplicationError(error.code())));
    } catch (const std::exception& error) {
        callback(errorResponse(error.what(), drogon::k500InternalServerError));
    }
}

}  // namespace schoolmanager::adapters
