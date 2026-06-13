#include "schoolmanager/adapters/ApiController.h"

#include "schoolmanager/adapters/JsonHelpers.h"
#include "schoolmanager/config/Constants.h"

#include <drogon/MultiPart.h>
#include <drogon/drogon.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

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

domain::FileContext fileContext(std::string contextType, std::string contextId)
{
    return domain::FileContext{
        .type = std::move(contextType),
        .id = std::move(contextId),
    };
}

std::optional<std::string> optionalParentId(std::string value)
{
    if (value.empty()) {
        return std::nullopt;
    }
    return value;
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

std::string fieldIdForStudentInfoValuePatch(std::string_view studentId, std::string_view fieldId)
{
    return "student_info_value:" + std::string(studentId) + ":" + std::string(fieldId) + ":value";
}

void emitStudentInfoMessage(const std::shared_ptr<BroadcastHub>& hub,
                            const std::string& type,
                            std::string_view studentId,
                            const domain::StudentInfoField& field,
                            const std::string& fieldId,
                            const Json::Value& changedFields = Json::Value(Json::arrayValue))
{
    Json::Value message(Json::objectValue);
    message["type"] = type;
    message["student_id"] = std::string(studentId);
    message["resource"] = "student_info";
    message["id"] = field.id;
    message["field_id"] = fieldId;
    message["info_field"] = toJson(field);
    message["changed_fields"] = changedFields;
    hub->broadcast(std::string(studentId), message);
}

std::string fieldIdForStudentInfoDefinitionPatch(
    std::string_view fieldId,
    const std::map<std::string, std::string>& changes)
{
    if (changes.size() == 1) {
        return "student_info_definition:" + std::string(fieldId) + ":" + changes.begin()->first;
    }
    return "student_info_definition:" + std::string(fieldId);
}

void emitStudentInfoDefinitionMessage(const std::shared_ptr<BroadcastHub>& hub,
                                      const std::string& type,
                                      const domain::StudentInfoDefinition& field,
                                      const std::string& fieldId,
                                      const Json::Value& changedFields =
                                          Json::Value(Json::arrayValue))
{
    Json::Value message(Json::objectValue);
    message["type"] = type;
    message["resource"] = "student_info_definition";
    message["id"] = field.id;
    message["field_id"] = fieldId;
    message["info_field_definition"] = toJson(field);
    message["changed_fields"] = changedFields;
    hub->broadcast(std::string(config::schoolWebSocketRoomId), message);
}

void emitStudentInfoDefinitionDeletedMessage(const std::shared_ptr<BroadcastHub>& hub,
                                             std::string_view fieldId)
{
    Json::Value message(Json::objectValue);
    message["type"] = "student_info_definition.deleted";
    message["resource"] = "student_info_definition";
    message["id"] = std::string(fieldId);
    message["field_id"] = "student_info_definition:" + std::string(fieldId);
    hub->broadcast(std::string(config::schoolWebSocketRoomId), message);
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

void emitStudentMessage(const std::shared_ptr<BroadcastHub>& hub,
                        const std::string& type,
                        const domain::StudentSummary& student,
                        const Json::Value& changedFields = Json::Value(Json::arrayValue))
{
    Json::Value message(Json::objectValue);
    message["type"] = type;
    message["resource"] = "student";
    message["id"] = student.id;
    message["field_id"] = "student:" + student.id;
    message["student"] = toJson(student);
    message["changed_fields"] = changedFields;
    hub->broadcast(std::string(config::schoolWebSocketRoomId), message);
}

void emitStudentDeletedMessage(const std::shared_ptr<BroadcastHub>& hub, std::string_view studentId)
{
    Json::Value message(Json::objectValue);
    message["type"] = "student.deleted";
    message["resource"] = "student";
    message["id"] = std::string(studentId);
    message["field_id"] = "student:" + std::string(studentId);
    hub->broadcast(std::string(config::schoolWebSocketRoomId), message);
}

void emitFileManagerChangedMessage(const std::shared_ptr<BroadcastHub>& hub,
                                   const domain::FileContext& context,
                                   std::string_view action)
{
    if (context.type != config::studentUploadsContextType) {
        return;
    }

    Json::Value message(Json::objectValue);
    message["type"] = std::string(config::fileManagerChangedMessageType);
    message["resource"] = std::string(config::fileManagerWebSocketResource);
    message["id"] = context.type + ":" + context.id;
    message["field_id"] = "file_manager:" + context.type + ":" + context.id;
    message["student_id"] = context.id;
    message["context_type"] = context.type;
    message["context_id"] = context.id;
    message["action"] = std::string(action);
    hub->broadcast(context.id, message);
}

std::vector<domain::StudentInfoField> mergeStudentInfoFields(
    const std::vector<domain::StudentInfoDefinition>& definitions,
    const std::vector<domain::StudentInfoValue>& values)
{
    std::unordered_map<std::string, domain::StudentInfoValue> valuesByFieldId;
    for (const auto& value : values) {
        valuesByFieldId.emplace(value.field_id, value);
    }

    std::vector<domain::StudentInfoField> fields;
    fields.reserve(definitions.size());
    for (const auto& definition : definitions) {
        domain::StudentInfoField field{
            .id = definition.id,
            .name = definition.name,
            .display_name = definition.display_name,
            .value_type = definition.value_type,
            .value = "",
            .updated_at = definition.updated_at,
        };

        const auto value = valuesByFieldId.find(definition.id);
        if (value != valuesByFieldId.end()) {
            if (domain::isValidStudentInfoValue(definition.value_type, value->second.value)) {
                field.value = value->second.value;
            }
            field.updated_at = std::max(definition.updated_at, value->second.updated_at);
        }

        fields.push_back(std::move(field));
    }
    return fields;
}

domain::StudentInfoField mergeStudentInfoField(
    const domain::StudentInfoDefinition& definition,
    const domain::StudentInfoValue& value)
{
    auto fields = mergeStudentInfoFields({definition}, {value});
    return fields.front();
}

}  // namespace

ApiController::ApiController(std::shared_ptr<infra::SchoolIndexRepository> schoolIndex,
                             std::shared_ptr<infra::StudentDataRepository> studentData,
                             std::shared_ptr<infra::FileManagerService> fileManager,
                             std::shared_ptr<BroadcastHub> broadcastHub,
                             std::filesystem::path openApiPath)
    : school_index_(std::move(schoolIndex)),
      student_data_(std::move(studentData)),
      file_manager_(std::move(fileManager)),
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

        emitStudentMessage(broadcast_hub_, "student.created", student);
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void ApiController::listStudentInfoDefinitions(const drogon::HttpRequestPtr&, Callback&& callback)
{
    try {
        Json::Value body(Json::objectValue);
        body["info_field_definitions"] = toJsonArray(school_index_->listStudentInfoDefinitions());
        callback(jsonResponse(std::move(body)));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void ApiController::createStudentInfoDefinition(const drogon::HttpRequestPtr& request,
                                                Callback&& callback)
{
    try {
        const auto json = request->getJsonObject();
        if (!json || !json->isObject() || !json->isMember("name")) {
            callback(errorResponse("name is required", drogon::k400BadRequest));
            return;
        }

        auto field = school_index_->createStudentInfoDefinition(
            bodyString(*json, "name"),
            bodyString(*json, "display_name"),
            bodyString(*json, "value_type", "STRING"));

        Json::Value responseBody(Json::objectValue);
        responseBody["info_field_definition"] = toJson(field);
        callback(jsonResponse(std::move(responseBody), drogon::k201Created));

        emitStudentInfoDefinitionMessage(broadcast_hub_,
                                         "student_info_definition.created",
                                         field,
                                         "student_info_definition:" + field.id);
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void ApiController::patchStudentInfoDefinition(const drogon::HttpRequestPtr& request,
                                               Callback&& callback,
                                               std::string fieldId)
{
    try {
        const auto json = request->getJsonObject();
        if (!json || !json->isObject()) {
            callback(errorResponse("JSON object body is required", drogon::k400BadRequest));
            return;
        }

        static const std::vector<std::string> allowedFields{
            "name", "display_name", "value_type"};
        std::map<std::string, std::string> changes;
        for (const auto& field : allowedFields) {
            if (json->isMember(field)) {
                changes.emplace(field, scalarToString((*json)[field]));
            }
        }
        if (changes.empty()) {
            callback(errorResponse("no supported student info definition fields provided",
                                   drogon::k400BadRequest));
            return;
        }

        const auto updated = school_index_->patchStudentInfoDefinition(fieldId, changes);
        if (!updated) {
            callback(errorResponse("student info field definition not found", drogon::k404NotFound));
            return;
        }

        Json::Value responseBody(Json::objectValue);
        responseBody["info_field_definition"] = toJson(*updated);
        responseBody["changed_fields"] = changedFieldsJson(changes);
        callback(jsonResponse(std::move(responseBody)));

        emitStudentInfoDefinitionMessage(broadcast_hub_,
                                         "student_info_definition.updated",
                                         *updated,
                                         fieldIdForStudentInfoDefinitionPatch(fieldId, changes),
                                         changedFieldsJson(changes));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void ApiController::deleteStudentInfoDefinition(const drogon::HttpRequestPtr&,
                                                Callback&& callback,
                                                std::string fieldId)
{
    try {
        if (!school_index_->deleteStudentInfoDefinition(fieldId)) {
            callback(errorResponse("student info field definition not found", drogon::k404NotFound));
            return;
        }

        for (const auto& student : school_index_->listStudents()) {
            student_data_->deleteStudentInfoValue(student.id, fieldId);
        }

        Json::Value body(Json::objectValue);
        body["deleted"] = true;
        callback(jsonResponse(std::move(body)));

        emitStudentInfoDefinitionDeletedMessage(broadcast_hub_, fieldId);
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
        body["info_fields"] = toJsonArray(mergeStudentInfoFields(
            school_index_->listStudentInfoDefinitions(),
            student_data_->listStudentInfoValues(studentId)));
        body["grades"] = toJsonArray(student_data_->listGrades(studentId));
        callback(jsonResponse(std::move(body)));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void ApiController::patchStudent(const drogon::HttpRequestPtr& request,
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
            school_index_->updateStudentDisplayName(studentId, scalarToString((*json)["display_name"]));
        if (!updated) {
            callback(errorResponse("student not found", drogon::k404NotFound));
            return;
        }

        Json::Value body(Json::objectValue);
        body["student"] = toJson(*updated);
        callback(jsonResponse(std::move(body)));

        Json::Value changedFields(Json::arrayValue);
        changedFields.append("display_name");
        emitStudentMessage(broadcast_hub_, "student.updated", *updated, changedFields);
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void ApiController::deleteStudent(const drogon::HttpRequestPtr&,
                                  Callback&& callback,
                                  std::string studentId)
{
    try {
        if (!school_index_->deleteStudent(studentId)) {
            callback(errorResponse("student not found", drogon::k404NotFound));
            return;
        }

        Json::Value body(Json::objectValue);
        body["deleted"] = true;
        callback(jsonResponse(std::move(body)));

        emitStudentDeletedMessage(broadcast_hub_, studentId);
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void ApiController::listStudentInfoFields(const drogon::HttpRequestPtr&,
                                          Callback&& callback,
                                          std::string studentId)
{
    try {
        if (!studentExists(studentId)) {
            callback(errorResponse("student not found", drogon::k404NotFound));
            return;
        }
        Json::Value body(Json::objectValue);
        body["info_fields"] = toJsonArray(mergeStudentInfoFields(
            school_index_->listStudentInfoDefinitions(),
            student_data_->listStudentInfoValues(studentId)));
        callback(jsonResponse(std::move(body)));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void ApiController::patchStudentInfoValue(const drogon::HttpRequestPtr& request,
                                          Callback&& callback,
                                          std::string studentId,
                                          std::string fieldId)
{
    try {
        if (!studentExists(studentId)) {
            callback(errorResponse("student not found", drogon::k404NotFound));
            return;
        }

        const auto json = request->getJsonObject();
        if (!json || !json->isObject() || !json->isMember("value")) {
            callback(errorResponse("value is required", drogon::k400BadRequest));
            return;
        }

        const auto definition = school_index_->getStudentInfoDefinition(fieldId);
        if (!definition) {
            callback(errorResponse("student info field definition not found", drogon::k404NotFound));
            return;
        }

        const auto updated = student_data_->patchStudentInfoValue(
            studentId,
            fieldId,
            definition->value_type,
            scalarToString((*json)["value"]));
        if (!updated) {
            callback(errorResponse("student info value not found", drogon::k404NotFound));
            return;
        }
        school_index_->touchStudent(studentId, updated->updated_at);

        const auto field = mergeStudentInfoField(*definition, *updated);
        Json::Value responseBody(Json::objectValue);
        responseBody["info_field"] = toJson(field);
        std::map<std::string, std::string> changes{{"value", field.value}};
        responseBody["changed_fields"] = changedFieldsJson(changes);
        callback(jsonResponse(std::move(responseBody)));

        emitStudentInfoMessage(broadcast_hub_,
                               "student_info.updated",
                               studentId,
                               field,
                               fieldIdForStudentInfoValuePatch(studentId, fieldId),
                               changedFieldsJson(changes));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
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

void ApiController::listFileEntries(const drogon::HttpRequestPtr& request,
                                    Callback&& callback,
                                    std::string contextType,
                                    std::string contextId)
{
    try {
        const auto entries = file_manager_->listEntries(
            fileContext(std::move(contextType), std::move(contextId)),
            optionalParentId(request->getParameter("parent_id")));

        Json::Value body(Json::objectValue);
        body["entries"] = toJsonArray(entries);
        callback(jsonResponse(std::move(body)));
    } catch (const infra::FileManagerError& e) {
        callback(errorResponse(e.what(), statusForFileManagerError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void ApiController::uploadFileEntry(const drogon::HttpRequestPtr& request,
                                    Callback&& callback,
                                    std::string contextType,
                                    std::string contextId)
{
    try {
        drogon::MultiPartParser parser;
        if (parser.parse(request) != 0 || parser.getFiles().empty()) {
            callback(errorResponse("multipart file field is required", drogon::k400BadRequest));
            return;
        }

        const auto& upload = parser.getFiles().front();
        auto context = fileContext(std::move(contextType), std::move(contextId));
        const auto entry = file_manager_->uploadFile(
            context,
            optionalParentId(request->getParameter("parent_id")),
            infra::FileUploadPayload{
                .file_name = upload.getFileName().empty() ? "uploaded_file" : upload.getFileName(),
                .mime_type = "application/octet-stream",
                .size_bytes = upload.fileLength(),
                .write_temp_file =
                    [&upload](const std::filesystem::path& path) {
                        if (upload.saveAs(path.string()) != 0) {
                            throw std::runtime_error("failed to write temporary upload file");
                        }
                    },
            });

        Json::Value body(Json::objectValue);
        body["entry"] = toJson(entry);
        callback(jsonResponse(std::move(body), drogon::k201Created));
        emitFileManagerChangedMessage(
            broadcast_hub_, context, config::fileManagerEntryCreatedAction);
    } catch (const infra::FileManagerError& e) {
        callback(errorResponse(e.what(), statusForFileManagerError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void ApiController::createFileFolder(const drogon::HttpRequestPtr& request,
                                     Callback&& callback,
                                     std::string contextType,
                                     std::string contextId)
{
    try {
        const auto json = request->getJsonObject();
        if (!json || !json->isObject() || !json->isMember("name")) {
            callback(errorResponse("name is required", drogon::k400BadRequest));
            return;
        }

        auto context = fileContext(std::move(contextType), std::move(contextId));
        const auto folder = file_manager_->createFolder(
            context,
            json->isMember("parent_id") ? optionalParentId(scalarToString((*json)["parent_id"]))
                                        : std::nullopt,
            scalarToString((*json)["name"]));

        Json::Value body(Json::objectValue);
        body["entry"] = toJson(folder);
        callback(jsonResponse(std::move(body), drogon::k201Created));
        emitFileManagerChangedMessage(
            broadcast_hub_, context, config::fileManagerEntryCreatedAction);
    } catch (const infra::FileManagerError& e) {
        callback(errorResponse(e.what(), statusForFileManagerError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void ApiController::renameFileEntry(const drogon::HttpRequestPtr& request,
                                    Callback&& callback,
                                    std::string contextType,
                                    std::string contextId,
                                    std::string entryId)
{
    try {
        const auto json = request->getJsonObject();
        if (!json || !json->isObject() || !json->isMember("name")) {
            callback(errorResponse("name is required", drogon::k400BadRequest));
            return;
        }

        auto context = fileContext(std::move(contextType), std::move(contextId));
        const auto entry = file_manager_->renameEntry(
            context,
            entryId,
            scalarToString((*json)["name"]));

        Json::Value body(Json::objectValue);
        body["entry"] = toJson(entry);
        callback(jsonResponse(std::move(body)));
        emitFileManagerChangedMessage(
            broadcast_hub_, context, config::fileManagerEntryUpdatedAction);
    } catch (const infra::FileManagerError& e) {
        callback(errorResponse(e.what(), statusForFileManagerError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void ApiController::moveFileEntryToTrash(const drogon::HttpRequestPtr&,
                                         Callback&& callback,
                                         std::string contextType,
                                         std::string contextId,
                                         std::string entryId)
{
    try {
        auto context = fileContext(std::move(contextType), std::move(contextId));
        file_manager_->moveEntryToTrash(context, entryId);

        Json::Value body(Json::objectValue);
        body["trashed"] = true;
        callback(jsonResponse(std::move(body)));
        emitFileManagerChangedMessage(
            broadcast_hub_, context, config::fileManagerEntryTrashedAction);
    } catch (const infra::FileManagerError& e) {
        callback(errorResponse(e.what(), statusForFileManagerError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void ApiController::downloadFileEntry(const drogon::HttpRequestPtr&,
                                      Callback&& callback,
                                      std::string contextType,
                                      std::string contextId,
                                      std::string entryId)
{
    try {
        const auto download = file_manager_->downloadFile(
            fileContext(std::move(contextType), std::move(contextId)), entryId);
        callback(drogon::HttpResponse::newFileResponse(download.path.string(), download.entry.name));
    } catch (const infra::FileManagerError& e) {
        callback(errorResponse(e.what(), statusForFileManagerError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void ApiController::listFileTrash(const drogon::HttpRequestPtr&,
                                  Callback&& callback,
                                  std::string contextType,
                                  std::string contextId)
{
    try {
        const auto trash =
            file_manager_->listTrash(fileContext(std::move(contextType), std::move(contextId)));

        Json::Value body(Json::objectValue);
        body["trash"] = toJsonArray(trash);
        callback(jsonResponse(std::move(body)));
    } catch (const infra::FileManagerError& e) {
        callback(errorResponse(e.what(), statusForFileManagerError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void ApiController::restoreFileTrashEntry(const drogon::HttpRequestPtr&,
                                          Callback&& callback,
                                          std::string contextType,
                                          std::string contextId,
                                          std::string trashEntryId)
{
    try {
        auto context = fileContext(std::move(contextType), std::move(contextId));
        file_manager_->restoreTrashEntry(context, trashEntryId);

        Json::Value body(Json::objectValue);
        body["restored"] = true;
        callback(jsonResponse(std::move(body)));
        emitFileManagerChangedMessage(
            broadcast_hub_, context, config::fileManagerTrashRestoredAction);
    } catch (const infra::FileManagerError& e) {
        callback(errorResponse(e.what(), statusForFileManagerError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void ApiController::permanentlyDeleteFileTrashEntry(const drogon::HttpRequestPtr&,
                                                    Callback&& callback,
                                                    std::string contextType,
                                                    std::string contextId,
                                                    std::string trashEntryId)
{
    try {
        auto context = fileContext(std::move(contextType), std::move(contextId));
        file_manager_->permanentlyDeleteTrashEntry(context, trashEntryId);

        Json::Value body(Json::objectValue);
        body["deleted"] = true;
        callback(jsonResponse(std::move(body)));
        emitFileManagerChangedMessage(
            broadcast_hub_, context, config::fileManagerTrashDeletedAction);
    } catch (const infra::FileManagerError& e) {
        callback(errorResponse(e.what(), statusForFileManagerError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

bool ApiController::studentExists(std::string_view studentId)
{
    return school_index_->getStudent(studentId).has_value();
}

}  // namespace schoolmanager::adapters
