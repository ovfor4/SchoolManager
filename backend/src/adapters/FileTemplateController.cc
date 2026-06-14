#include "schoolmanager/adapters/FileTemplateController.h"

#include "schoolmanager/adapters/HttpJson.h"
#include "schoolmanager/adapters/JsonHelpers.h"

#include <utility>

namespace schoolmanager::adapters {

namespace {

using Callback = std::function<void(const drogon::HttpResponsePtr&)>;

application::FileTemplateGenerateRequest parseGenerateRequest(const Json::Value& body)
{
    if (!body.isObject() || !body.isMember("template_entry_id") ||
        !body.isMember("student_ids") || !body["student_ids"].isArray()) {
        throw application::ApplicationError(
            application::ApplicationErrorCode::BadRequest,
            "template_entry_id and student_ids are required");
    }

    application::FileTemplateGenerateRequest request;
    request.template_entry_id = scalarToString(body["template_entry_id"]);
    for (const auto& studentId : body["student_ids"]) {
        if (!studentId.isString()) {
            throw application::ApplicationError(
                application::ApplicationErrorCode::BadRequest,
                "student_ids must be an array of strings");
        }
        request.student_ids.push_back(studentId.asString());
    }
    return request;
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
