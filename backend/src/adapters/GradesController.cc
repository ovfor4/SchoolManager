#include "schoolmanager/adapters/GradesController.h"

#include "schoolmanager/adapters/HttpJson.h"
#include "schoolmanager/adapters/JsonHelpers.h"

#include <utility>

namespace schoolmanager::adapters {

namespace {

using Callback = std::function<void(const drogon::HttpResponsePtr&)>;

}  // namespace

GradesController::GradesController(std::shared_ptr<application::GradeUseCases> grades,
                                   std::shared_ptr<RealtimePublisher> realtime)
    : grades_(std::move(grades)), realtime_(std::move(realtime))
{
}

void GradesController::listGrades(const drogon::HttpRequestPtr&,
                                  Callback&& callback,
                                  std::string studentId)
{
    try {
        Json::Value body(Json::objectValue);
        body["grades"] = toJsonArray(grades_->listGrades(studentId));
        callback(jsonResponse(std::move(body)));
    } catch (const application::ApplicationError& e) {
        callback(errorResponse(e.what(), statusForApplicationError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void GradesController::createGrade(const drogon::HttpRequestPtr& request,
                                   Callback&& callback,
                                   std::string studentId)
{
    try {
        const auto json = request->getJsonObject();
        const Json::Value empty(Json::objectValue);
        const auto& body = json ? *json : empty;

        auto grade = grades_->createGrade(studentId,
                                          application::GradeCreateInput{
                                              .title = bodyString(body, "title", "Untitled grade"),
                                              .score = bodyString(body, "score"),
                                              .max_score = bodyString(body, "max_score"),
                                              .occurred_on = bodyString(body, "occurred_on"),
                                              .notes = bodyString(body, "notes"),
                                          });

        Json::Value responseBody(Json::objectValue);
        responseBody["grade"] = toJson(grade);
        callback(jsonResponse(std::move(responseBody), drogon::k201Created));

        realtime_->gradeCreated(studentId, grade);
    } catch (const application::ApplicationError& e) {
        callback(errorResponse(e.what(), statusForApplicationError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void GradesController::patchGrade(const drogon::HttpRequestPtr& request,
                                  Callback&& callback,
                                  std::string studentId,
                                  std::string gradeId)
{
    try {
        const auto json = request->getJsonObject();
        if (!json || !json->isObject()) {
            callback(errorResponse("JSON object body is required", drogon::k400BadRequest));
            return;
        }

        const auto changes =
            collectStringFields(*json, {"title", "score", "max_score", "occurred_on", "notes"});
        if (changes.empty()) {
            callback(errorResponse("no supported grade fields provided", drogon::k400BadRequest));
            return;
        }

        const auto updated = grades_->patchGrade(studentId, gradeId, changes);
        Json::Value body(Json::objectValue);
        body["grade"] = toJson(updated);
        body["changed_fields"] = changedFieldsJson(changes);
        callback(jsonResponse(std::move(body)));

        realtime_->gradeUpdated(studentId, updated, changes);
    } catch (const application::ApplicationError& e) {
        callback(errorResponse(e.what(), statusForApplicationError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void GradesController::deleteGrade(const drogon::HttpRequestPtr&,
                                   Callback&& callback,
                                   std::string studentId,
                                   std::string gradeId)
{
    try {
        grades_->deleteGrade(studentId, gradeId);

        Json::Value body(Json::objectValue);
        body["deleted"] = true;
        callback(jsonResponse(std::move(body)));

        realtime_->gradeDeleted(studentId, gradeId);
    } catch (const application::ApplicationError& e) {
        callback(errorResponse(e.what(), statusForApplicationError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

}  // namespace schoolmanager::adapters
