#include "schoolmanager/adapters/StudentInfoController.h"

#include "schoolmanager/adapters/HttpJson.h"
#include "schoolmanager/adapters/JsonHelpers.h"

#include <utility>

namespace schoolmanager::adapters {

namespace {

using Callback = std::function<void(const drogon::HttpResponsePtr&)>;

}  // namespace

StudentInfoController::StudentInfoController(
    std::shared_ptr<application::StudentInfoUseCases> studentInfo,
    std::shared_ptr<RealtimePublisher> realtime)
    : student_info_(std::move(studentInfo)), realtime_(std::move(realtime))
{
}

void StudentInfoController::listDefinitions(const drogon::HttpRequestPtr&, Callback&& callback)
{
    try {
        Json::Value body(Json::objectValue);
        body["info_field_definitions"] = toJsonArray(student_info_->listDefinitions());
        callback(jsonResponse(std::move(body)));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void StudentInfoController::createDefinition(const drogon::HttpRequestPtr& request,
                                             Callback&& callback)
{
    try {
        const auto json = request->getJsonObject();
        if (!json || !json->isObject() || !json->isMember("name")) {
            callback(errorResponse("name is required", drogon::k400BadRequest));
            return;
        }

        auto field = student_info_->createDefinition(bodyString(*json, "name"),
                                                     bodyString(*json, "display_name"),
                                                     bodyString(*json, "value_type", "STRING"));

        Json::Value body(Json::objectValue);
        body["info_field_definition"] = toJson(field);
        callback(jsonResponse(std::move(body), drogon::k201Created));

        realtime_->studentInfoDefinitionCreated(field);
    } catch (const application::ApplicationError& e) {
        callback(errorResponse(e.what(), statusForApplicationError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void StudentInfoController::patchDefinition(const drogon::HttpRequestPtr& request,
                                            Callback&& callback,
                                            std::string fieldId)
{
    try {
        const auto json = request->getJsonObject();
        if (!json || !json->isObject()) {
            callback(errorResponse("JSON object body is required", drogon::k400BadRequest));
            return;
        }

        const auto changes = collectStringFields(*json, {"name", "display_name", "value_type"});
        if (changes.empty()) {
            callback(errorResponse("no supported student info definition fields provided",
                                   drogon::k400BadRequest));
            return;
        }

        const auto updated = student_info_->patchDefinition(fieldId, changes);
        Json::Value body(Json::objectValue);
        body["info_field_definition"] = toJson(updated);
        body["changed_fields"] = changedFieldsJson(changes);
        callback(jsonResponse(std::move(body)));

        realtime_->studentInfoDefinitionUpdated(updated, changes);
    } catch (const application::ApplicationError& e) {
        callback(errorResponse(e.what(), statusForApplicationError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void StudentInfoController::deleteDefinition(const drogon::HttpRequestPtr&,
                                             Callback&& callback,
                                             std::string fieldId)
{
    try {
        student_info_->deleteDefinition(fieldId);

        Json::Value body(Json::objectValue);
        body["deleted"] = true;
        callback(jsonResponse(std::move(body)));

        realtime_->studentInfoDefinitionDeleted(fieldId);
    } catch (const application::ApplicationError& e) {
        callback(errorResponse(e.what(), statusForApplicationError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void StudentInfoController::listStudentFields(const drogon::HttpRequestPtr&,
                                              Callback&& callback,
                                              std::string studentId)
{
    try {
        Json::Value body(Json::objectValue);
        body["info_fields"] = toJsonArray(student_info_->listStudentFields(studentId));
        callback(jsonResponse(std::move(body)));
    } catch (const application::ApplicationError& e) {
        callback(errorResponse(e.what(), statusForApplicationError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void StudentInfoController::patchStudentValue(const drogon::HttpRequestPtr& request,
                                              Callback&& callback,
                                              std::string studentId,
                                              std::string fieldId)
{
    try {
        const auto json = request->getJsonObject();
        if (!json || !json->isObject() || !json->isMember("value")) {
            callback(errorResponse("value is required", drogon::k400BadRequest));
            return;
        }

        const auto field =
            student_info_->patchStudentValue(studentId, fieldId, scalarToString((*json)["value"]));
        std::map<std::string, std::string> changes{{"value", field.value}};

        Json::Value body(Json::objectValue);
        body["info_field"] = toJson(field);
        body["changed_fields"] = changedFieldsJson(changes);
        callback(jsonResponse(std::move(body)));

        realtime_->studentInfoUpdated(studentId, field, changes);
    } catch (const application::ApplicationError& e) {
        callback(errorResponse(e.what(), statusForApplicationError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

}  // namespace schoolmanager::adapters
