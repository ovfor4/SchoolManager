#include "schoolmanager/adapters/RealtimePublisher.h"

#include "schoolmanager/adapters/HttpJson.h"
#include "schoolmanager/adapters/JsonHelpers.h"
#include "schoolmanager/config/Constants.h"

#include <utility>

namespace schoolmanager::adapters {

namespace {

std::string gradeFieldIdForPatch(std::string_view gradeId,
                                 const std::map<std::string, std::string>& changes)
{
    if (changes.size() == 1) {
        return "grade:" + std::string(gradeId) + ":" + changes.begin()->first;
    }
    return "grade:" + std::string(gradeId);
}

std::string studentInfoValueFieldId(std::string_view studentId, std::string_view fieldId)
{
    return "student_info_value:" + std::string(studentId) + ":" + std::string(fieldId) +
           ":value";
}

std::string studentInfoDefinitionFieldIdForPatch(
    std::string_view fieldId,
    const std::map<std::string, std::string>& changes)
{
    if (changes.size() == 1) {
        return "student_info_definition:" + std::string(fieldId) + ":" +
               changes.begin()->first;
    }
    return "student_info_definition:" + std::string(fieldId);
}

}  // namespace

RealtimePublisher::RealtimePublisher(std::shared_ptr<BroadcastHub> hub) : hub_(std::move(hub))
{
}

void RealtimePublisher::studentCreated(const domain::StudentSummary& student)
{
    Json::Value changes(Json::arrayValue);
    Json::Value message(Json::objectValue);
    message["type"] = "student.created";
    message["resource"] = "student";
    message["id"] = student.id;
    message["field_id"] = "student:" + student.id;
    message["student"] = toJson(student);
    message["changed_fields"] = changes;
    hub_->broadcast(std::string(config::schoolWebSocketRoomId), message);
}

void RealtimePublisher::studentUpdated(const domain::StudentSummary& student,
                                       const std::map<std::string, std::string>& changes)
{
    Json::Value message(Json::objectValue);
    message["type"] = "student.updated";
    message["resource"] = "student";
    message["id"] = student.id;
    message["field_id"] = "student:" + student.id;
    message["student"] = toJson(student);
    message["changed_fields"] = changedFieldsJson(changes);
    hub_->broadcast(std::string(config::schoolWebSocketRoomId), message);
}

void RealtimePublisher::studentDeleted(std::string_view studentId)
{
    Json::Value message(Json::objectValue);
    message["type"] = "student.deleted";
    message["resource"] = "student";
    message["id"] = std::string(studentId);
    message["field_id"] = "student:" + std::string(studentId);
    hub_->broadcast(std::string(config::schoolWebSocketRoomId), message);
}

void RealtimePublisher::studentInfoDefinitionCreated(
    const domain::StudentInfoDefinition& field)
{
    Json::Value changes(Json::arrayValue);
    Json::Value message(Json::objectValue);
    message["type"] = "student_info_definition.created";
    message["resource"] = "student_info_definition";
    message["id"] = field.id;
    message["field_id"] = "student_info_definition:" + field.id;
    message["info_field_definition"] = toJson(field);
    message["changed_fields"] = changes;
    hub_->broadcast(std::string(config::schoolWebSocketRoomId), message);
}

void RealtimePublisher::studentInfoDefinitionUpdated(
    const domain::StudentInfoDefinition& field,
    const std::map<std::string, std::string>& changes)
{
    Json::Value message(Json::objectValue);
    message["type"] = "student_info_definition.updated";
    message["resource"] = "student_info_definition";
    message["id"] = field.id;
    message["field_id"] = studentInfoDefinitionFieldIdForPatch(field.id, changes);
    message["info_field_definition"] = toJson(field);
    message["changed_fields"] = changedFieldsJson(changes);
    hub_->broadcast(std::string(config::schoolWebSocketRoomId), message);
}

void RealtimePublisher::studentInfoDefinitionDeleted(std::string_view fieldId)
{
    Json::Value message(Json::objectValue);
    message["type"] = "student_info_definition.deleted";
    message["resource"] = "student_info_definition";
    message["id"] = std::string(fieldId);
    message["field_id"] = "student_info_definition:" + std::string(fieldId);
    hub_->broadcast(std::string(config::schoolWebSocketRoomId), message);
}

void RealtimePublisher::studentInfoUpdated(
    std::string_view studentId,
    const domain::StudentInfoField& field,
    const std::map<std::string, std::string>& changes)
{
    Json::Value message(Json::objectValue);
    message["type"] = "student_info.updated";
    message["student_id"] = std::string(studentId);
    message["resource"] = "student_info";
    message["id"] = field.id;
    message["field_id"] = studentInfoValueFieldId(studentId, field.id);
    message["info_field"] = toJson(field);
    message["changed_fields"] = changedFieldsJson(changes);
    hub_->broadcast(std::string(studentId), message);
}

void RealtimePublisher::gradeCreated(std::string_view studentId, const domain::Grade& grade)
{
    Json::Value changes(Json::arrayValue);
    Json::Value message(Json::objectValue);
    message["type"] = "grade.created";
    message["student_id"] = std::string(studentId);
    message["resource"] = "grade";
    message["id"] = grade.id;
    message["field_id"] = "grade:" + grade.id;
    message["grade"] = toJson(grade);
    message["changed_fields"] = changes;
    hub_->broadcast(std::string(studentId), message);
}

void RealtimePublisher::gradeUpdated(std::string_view studentId,
                                     const domain::Grade& grade,
                                     const std::map<std::string, std::string>& changes)
{
    Json::Value message(Json::objectValue);
    message["type"] = "grade.updated";
    message["student_id"] = std::string(studentId);
    message["resource"] = "grade";
    message["id"] = grade.id;
    message["field_id"] = gradeFieldIdForPatch(grade.id, changes);
    message["grade"] = toJson(grade);
    message["changed_fields"] = changedFieldsJson(changes);
    hub_->broadcast(std::string(studentId), message);
}

void RealtimePublisher::gradeDeleted(std::string_view studentId, std::string_view gradeId)
{
    Json::Value message(Json::objectValue);
    message["type"] = "grade.deleted";
    message["student_id"] = std::string(studentId);
    message["resource"] = "grade";
    message["id"] = std::string(gradeId);
    message["field_id"] = "grade:" + std::string(gradeId);
    hub_->broadcast(std::string(studentId), message);
}

void RealtimePublisher::fileManagerChanged(const domain::FileContext& context,
                                           std::string_view action)
{
    if (context.type != config::studentUploadsContextType &&
        !(context.type == config::globalTemplatesContextType &&
          context.id == config::defaultGlobalTemplatesContextId)) {
        return;
    }

    Json::Value message(Json::objectValue);
    message["type"] = std::string(config::fileManagerChangedMessageType);
    message["resource"] = std::string(config::fileManagerWebSocketResource);
    message["id"] = context.type + ":" + context.id;
    message["field_id"] = "file_manager:" + context.type + ":" + context.id;
    message["context_type"] = context.type;
    message["context_id"] = context.id;
    message["action"] = std::string(action);

    if (context.type == config::studentUploadsContextType) {
        message["student_id"] = context.id;
        hub_->broadcast(context.id, message);
        return;
    }

    hub_->broadcast(std::string(config::schoolWebSocketRoomId), message);
}

}  // namespace schoolmanager::adapters
