#include "schoolmanager/adapters/StudentsController.h"

#include "schoolmanager/adapters/HttpJson.h"
#include "schoolmanager/adapters/JsonHelpers.h"

#include <map>
#include <utility>

namespace schoolmanager::adapters {

namespace {

using Callback = std::function<void(const drogon::HttpResponsePtr&)>;

Json::Value studentDetailJson(const domain::StudentDetail& detail)
{
    Json::Value body(Json::objectValue);
    body["student"] = toJson(detail.student);
    body["info_fields"] = toJsonArray(detail.info_fields);
    body["grades"] = toJsonArray(detail.grades);
    return body;
}

}  // namespace

StudentsController::StudentsController(std::shared_ptr<application::StudentUseCases> students,
                                       std::shared_ptr<RealtimePublisher> realtime)
    : students_(std::move(students)), realtime_(std::move(realtime))
{
}

void StudentsController::listStudents(const drogon::HttpRequestPtr&, Callback&& callback)
{
    try {
        Json::Value body(Json::objectValue);
        body["students"] = toJsonArray(students_->listStudents());
        callback(jsonResponse(std::move(body)));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void StudentsController::createStudent(const drogon::HttpRequestPtr& request, Callback&& callback)
{
    try {
        const auto json = request->getJsonObject();
        const std::string displayName =
            json ? bodyString(*json, "display_name", "Unnamed student") : "Unnamed student";

        auto student = students_->createStudent(displayName);
        Json::Value body(Json::objectValue);
        body["student"] = toJson(student);
        callback(jsonResponse(std::move(body), drogon::k201Created));

        realtime_->studentCreated(student);
    } catch (const application::ApplicationError& e) {
        callback(errorResponse(e.what(), statusForApplicationError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void StudentsController::getStudent(const drogon::HttpRequestPtr&,
                                    Callback&& callback,
                                    std::string studentId)
{
    try {
        callback(jsonResponse(studentDetailJson(students_->getStudentDetail(studentId))));
    } catch (const application::ApplicationError& e) {
        callback(errorResponse(e.what(), statusForApplicationError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void StudentsController::patchStudent(const drogon::HttpRequestPtr& request,
                                      Callback&& callback,
                                      std::string studentId)
{
    try {
        const auto json = request->getJsonObject();
        if (!json || !json->isObject() || !json->isMember("display_name")) {
            callback(errorResponse("display_name is required", drogon::k400BadRequest));
            return;
        }

        const auto updated =
            students_->updateStudentDisplayName(studentId, scalarToString((*json)["display_name"]));
        Json::Value body(Json::objectValue);
        body["student"] = toJson(updated);
        callback(jsonResponse(std::move(body)));

        realtime_->studentUpdated(updated, {{"display_name", updated.display_name}});
    } catch (const application::ApplicationError& e) {
        callback(errorResponse(e.what(), statusForApplicationError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void StudentsController::deleteStudent(const drogon::HttpRequestPtr&,
                                       Callback&& callback,
                                       std::string studentId)
{
    try {
        students_->deleteStudent(studentId);

        Json::Value body(Json::objectValue);
        body["deleted"] = true;
        callback(jsonResponse(std::move(body)));

        realtime_->studentDeleted(studentId);
    } catch (const application::ApplicationError& e) {
        callback(errorResponse(e.what(), statusForApplicationError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

}  // namespace schoolmanager::adapters
