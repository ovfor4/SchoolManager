#include "schoolmanager/adapters/ApiController.h"

#include "schoolmanager/adapters/JsonHelpers.h"
#include "schoolmanager/infra/StoragePaths.h"

#include <drogon/MultiPart.h>
#include <drogon/drogon.h>

#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace schoolmanager::adapters {

namespace {

using Callback = std::function<void(const drogon::HttpResponsePtr&)>;

drogon::HttpResponsePtr jsonResponse(Json::Value body,
                                     drogon::HttpStatusCode status = drogon::k200OK)
{
    auto response = drogon::HttpResponse::newHttpJsonResponse(std::move(body));
    response->setStatusCode(status);
    return response;
}

drogon::HttpResponsePtr errorResponse(const std::string& message,
                                      drogon::HttpStatusCode status)
{
    Json::Value body(Json::objectValue);
    body["error"] = message;
    return jsonResponse(std::move(body), status);
}

std::string bodyString(const Json::Value& body,
                       const std::string& field,
                       const std::string& fallback = "")
{
    if (!body.isMember(field)) {
        return fallback;
    }
    return scalarToString(body[field]);
}

Json::Value changedFieldsJson(const std::map<std::string, std::string>& changes)
{
    Json::Value fields(Json::arrayValue);
    for (const auto& [field, _] : changes) {
        fields.append(field);
    }
    return fields;
}

std::string fieldIdForPatch(std::string_view gradeId,
                            const std::map<std::string, std::string>& changes)
{
    if (changes.size() == 1) {
        return "grade:" + std::string(gradeId) + ":" + changes.begin()->first;
    }
    return "grade:" + std::string(gradeId);
}

void emitGradeMessage(const std::shared_ptr<BroadcastHub>& hub,
                      const std::string& type,
                      std::string_view studentId,
                      const domain::Grade& grade,
                      const std::string& fieldId,
                      const Json::Value& changedFields = Json::Value(Json::arrayValue))
{
    Json::Value message(Json::objectValue);
    message["type"] = type;
    message["student_id"] = std::string(studentId);
    message["resource"] = "grade";
    message["id"] = grade.id;
    message["field_id"] = fieldId;
    message["grade"] = toJson(grade);
    message["changed_fields"] = changedFields;
    hub->broadcast(std::string(studentId), message);
}

}  // namespace

ApiController::ApiController(std::shared_ptr<infra::SchoolIndexRepository> schoolIndex,
                             std::shared_ptr<infra::StudentDataRepository> studentData,
                             std::shared_ptr<BroadcastHub> broadcastHub,
                             std::filesystem::path openApiPath)
    : school_index_(std::move(schoolIndex)),
      student_data_(std::move(studentData)),
      broadcast_hub_(std::move(broadcastHub)),
      open_api_path_(std::move(openApiPath))
{
}

void ApiController::health(const drogon::HttpRequestPtr&, Callback&& callback)
{
    Json::Value body(Json::objectValue);
    body["status"] = "ok";
    body["service"] = "schoolmanager_backend";
    callback(jsonResponse(std::move(body)));
}

void ApiController::openApi(const drogon::HttpRequestPtr&, Callback&& callback)
{
    std::ifstream input(open_api_path_);
    if (!input) {
        callback(errorResponse("openapi contract file not found", drogon::k500InternalServerError));
        return;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    auto response = drogon::HttpResponse::newHttpResponse();
    response->setStatusCode(drogon::k200OK);
    response->setContentTypeString("application/json");
    response->setBody(buffer.str());
    callback(response);
}

void ApiController::listStudents(const drogon::HttpRequestPtr&, Callback&& callback)
{
    try {
        Json::Value body(Json::objectValue);
        body["students"] = toJsonArray(school_index_->listStudents());
        callback(jsonResponse(std::move(body)));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void ApiController::createStudent(const drogon::HttpRequestPtr& request, Callback&& callback)
{
    try {
        const auto json = request->getJsonObject();
        const std::string displayName =
            json ? bodyString(*json, "display_name", "Unnamed student") : "Unnamed student";

        auto student = school_index_->createStudent(displayName);
        student_data_->initializeStudent(student.id);

        Json::Value body(Json::objectValue);
        body["student"] = toJson(student);
        callback(jsonResponse(std::move(body), drogon::k201Created));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void ApiController::getStudent(const drogon::HttpRequestPtr&, Callback&& callback, std::string studentId)
{
    try {
        const auto student = school_index_->getStudent(studentId);
        if (!student) {
            callback(errorResponse("student not found", drogon::k404NotFound));
            return;
        }

        Json::Value body(Json::objectValue);
        body["student"] = toJson(*student);
        body["grades"] = toJsonArray(student_data_->listGrades(studentId));
        body["files"] = toJsonArray(student_data_->listFiles(studentId));
        callback(jsonResponse(std::move(body)));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void ApiController::listGrades(const drogon::HttpRequestPtr&, Callback&& callback, std::string studentId)
{
    try {
        if (!studentExists(studentId)) {
            callback(errorResponse("student not found", drogon::k404NotFound));
            return;
        }
        Json::Value body(Json::objectValue);
        body["grades"] = toJsonArray(student_data_->listGrades(studentId));
        callback(jsonResponse(std::move(body)));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void ApiController::createGrade(const drogon::HttpRequestPtr& request,
                                Callback&& callback,
                                std::string studentId)
{
    try {
        if (!studentExists(studentId)) {
            callback(errorResponse("student not found", drogon::k404NotFound));
            return;
        }
        const auto json = request->getJsonObject();
        const Json::Value empty(Json::objectValue);
        const auto& body = json ? *json : empty;

        auto grade = student_data_->createGrade(studentId,
                                                bodyString(body, "title", "Untitled grade"),
                                                bodyString(body, "score"),
                                                bodyString(body, "max_score"),
                                                bodyString(body, "occurred_on"),
                                                bodyString(body, "notes"));
        school_index_->touchStudent(studentId, grade.updated_at);

        Json::Value responseBody(Json::objectValue);
        responseBody["grade"] = toJson(grade);
        callback(jsonResponse(std::move(responseBody), drogon::k201Created));

        emitGradeMessage(broadcast_hub_, "grade.created", studentId, grade, "grade:" + grade.id);
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void ApiController::patchGrade(const drogon::HttpRequestPtr& request,
                               Callback&& callback,
                               std::string studentId,
                               std::string gradeId)
{
    try {
        if (!studentExists(studentId)) {
            callback(errorResponse("student not found", drogon::k404NotFound));
            return;
        }

        const auto json = request->getJsonObject();
        if (!json || !json->isObject()) {
            callback(errorResponse("JSON object body is required", drogon::k400BadRequest));
            return;
        }

        static const std::vector<std::string> allowedFields{
            "title", "score", "max_score", "occurred_on", "notes"};
        std::map<std::string, std::string> changes;
        for (const auto& field : allowedFields) {
            if (json->isMember(field)) {
                changes.emplace(field, scalarToString((*json)[field]));
            }
        }
        if (changes.empty()) {
            callback(errorResponse("no supported grade fields provided", drogon::k400BadRequest));
            return;
        }

        const auto updated = student_data_->patchGrade(studentId, gradeId, changes);
        if (!updated) {
            callback(errorResponse("grade not found", drogon::k404NotFound));
            return;
        }
        school_index_->touchStudent(studentId, updated->updated_at);

        Json::Value responseBody(Json::objectValue);
        responseBody["grade"] = toJson(*updated);
        responseBody["changed_fields"] = changedFieldsJson(changes);
        callback(jsonResponse(std::move(responseBody)));

        emitGradeMessage(broadcast_hub_,
                         "grade.updated",
                         studentId,
                         *updated,
                         fieldIdForPatch(gradeId, changes),
                         changedFieldsJson(changes));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void ApiController::deleteGrade(const drogon::HttpRequestPtr&,
                                Callback&& callback,
                                std::string studentId,
                                std::string gradeId)
{
    try {
        if (!studentExists(studentId)) {
            callback(errorResponse("student not found", drogon::k404NotFound));
            return;
        }
        if (!student_data_->deleteGrade(studentId, gradeId)) {
            callback(errorResponse("grade not found", drogon::k404NotFound));
            return;
        }
        const auto now = domain::unixTimeMillis();
        school_index_->touchStudent(studentId, now);

        Json::Value body(Json::objectValue);
        body["deleted"] = true;
        callback(jsonResponse(std::move(body)));

        Json::Value message(Json::objectValue);
        message["type"] = "grade.deleted";
        message["student_id"] = studentId;
        message["resource"] = "grade";
        message["id"] = gradeId;
        message["field_id"] = "grade:" + gradeId;
        broadcast_hub_->broadcast(studentId, message);
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void ApiController::listFiles(const drogon::HttpRequestPtr&, Callback&& callback, std::string studentId)
{
    try {
        if (!studentExists(studentId)) {
            callback(errorResponse("student not found", drogon::k404NotFound));
            return;
        }
        Json::Value body(Json::objectValue);
        body["files"] = toJsonArray(student_data_->listFiles(studentId));
        callback(jsonResponse(std::move(body)));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void ApiController::uploadFile(const drogon::HttpRequestPtr& request,
                               Callback&& callback,
                               std::string studentId)
{
    try {
        if (!studentExists(studentId)) {
            callback(errorResponse("student not found", drogon::k404NotFound));
            return;
        }

        drogon::MultiPartParser parser;
        if (parser.parse(request) != 0 || parser.getFiles().empty()) {
            callback(errorResponse("multipart file field is required", drogon::k400BadRequest));
            return;
        }

        const auto& upload = parser.getFiles().front();
        const auto originalName = upload.getFileName().empty() ? "uploaded_file" : upload.getFileName();
        const auto storedName = domain::createId() + "_" + infra::sanitizeFileName(originalName);
        const auto uploadDir = student_data_->uploadsDir(studentId);
        std::filesystem::create_directories(uploadDir);
        const auto finalPath = student_data_->filePath(studentId, storedName);
        auto tmpPath = finalPath;
        tmpPath += ".tmp";

        {
            std::ofstream output(tmpPath, std::ios::binary | std::ios::trunc);
            if (!output) {
                throw std::runtime_error("failed to open temporary upload file");
            }
            output.write(upload.fileData(), static_cast<std::streamsize>(upload.fileLength()));
            if (!output) {
                throw std::runtime_error("failed to write temporary upload file");
            }
        }

        auto pending = student_data_->createPendingFile(studentId,
                                                        originalName,
                                                        storedName,
                                                        "application/octet-stream",
                                                        upload.fileLength());
        try {
            std::filesystem::rename(tmpPath, finalPath);
        } catch (...) {
            student_data_->deleteFileRecord(studentId, pending.id);
            std::error_code ignored;
            std::filesystem::remove(tmpPath, ignored);
            throw;
        }

        const auto active = student_data_->activateFile(studentId, pending.id);
        if (!active) {
            throw std::runtime_error("failed to activate uploaded file");
        }
        school_index_->touchStudent(studentId, active->updated_at);

        Json::Value body(Json::objectValue);
        body["file"] = toJson(*active);
        callback(jsonResponse(std::move(body), drogon::k201Created));

        Json::Value message(Json::objectValue);
        message["type"] = "file.created";
        message["student_id"] = studentId;
        message["resource"] = "file";
        message["id"] = active->id;
        message["field_id"] = "file:" + active->id;
        message["file"] = toJson(*active);
        broadcast_hub_->broadcast(studentId, message);
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void ApiController::downloadFile(const drogon::HttpRequestPtr&,
                                 Callback&& callback,
                                 std::string studentId,
                                 std::string fileId)
{
    try {
        if (!studentExists(studentId)) {
            callback(errorResponse("student not found", drogon::k404NotFound));
            return;
        }
        const auto file = student_data_->getFile(studentId, fileId);
        if (!file || file->status != "active") {
            callback(errorResponse("file not found", drogon::k404NotFound));
            return;
        }

        const auto path = student_data_->filePath(studentId, file->stored_name);
        if (!std::filesystem::exists(path)) {
            callback(errorResponse("file content missing", drogon::k404NotFound));
            return;
        }

        callback(drogon::HttpResponse::newFileResponse(path.string(), file->original_name));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

bool ApiController::studentExists(std::string_view studentId)
{
    return school_index_->getStudent(studentId).has_value();
}

}  // namespace schoolmanager::adapters
