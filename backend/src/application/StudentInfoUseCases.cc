#include "schoolmanager/application/StudentInfoUseCases.h"

#include "schoolmanager/application/ApplicationError.h"
#include "schoolmanager/application/StudentInfoFields.h"

#include <utility>

namespace schoolmanager::application {

StudentInfoUseCases::StudentInfoUseCases(
    std::shared_ptr<infra::SchoolIndexRepository> schoolIndex,
    std::shared_ptr<infra::StudentDataRepository> studentData)
    : school_index_(std::move(schoolIndex)), student_data_(std::move(studentData))
{
}

std::vector<domain::StudentInfoDefinition> StudentInfoUseCases::listDefinitions()
{
    return school_index_->listStudentInfoDefinitions();
}

domain::StudentInfoDefinition StudentInfoUseCases::createDefinition(std::string name,
                                                                    std::string displayName,
                                                                    std::string valueType)
{
    return school_index_->createStudentInfoDefinition(
        std::move(name),
        std::move(displayName),
        std::move(valueType));
}

domain::StudentInfoDefinition StudentInfoUseCases::patchDefinition(
    std::string_view fieldId,
    const std::map<std::string, std::string>& changes)
{
    if (changes.empty()) {
        throw ApplicationError(ApplicationErrorCode::BadRequest,
                               "no supported student info definition fields provided");
    }

    const auto updated = school_index_->patchStudentInfoDefinition(fieldId, changes);
    if (!updated) {
        throw ApplicationError(ApplicationErrorCode::NotFound,
                               "student info field definition not found");
    }
    return *updated;
}

void StudentInfoUseCases::deleteDefinition(std::string_view fieldId)
{
    if (!school_index_->deleteStudentInfoDefinition(fieldId)) {
        throw ApplicationError(ApplicationErrorCode::NotFound,
                               "student info field definition not found");
    }

    for (const auto& student : school_index_->listStudents()) {
        student_data_->deleteStudentInfoValue(student.id, fieldId);
    }
}

std::vector<domain::StudentInfoField> StudentInfoUseCases::listStudentFields(
    std::string_view studentId)
{
    ensureStudentExists(studentId);
    return mergeStudentInfoFields(school_index_->listStudentInfoDefinitions(),
                                  student_data_->listStudentInfoValues(studentId));
}

domain::StudentInfoField StudentInfoUseCases::patchStudentValue(std::string_view studentId,
                                                                std::string_view fieldId,
                                                                std::string value)
{
    ensureStudentExists(studentId);

    const auto definition = school_index_->getStudentInfoDefinition(fieldId);
    if (!definition) {
        throw ApplicationError(ApplicationErrorCode::NotFound,
                               "student info field definition not found");
    }

    const auto updated = student_data_->patchStudentInfoValue(
        studentId,
        fieldId,
        definition->value_type,
        std::move(value));
    if (!updated) {
        throw ApplicationError(ApplicationErrorCode::NotFound, "student info value not found");
    }

    school_index_->touchStudent(studentId, updated->updated_at);
    return mergeStudentInfoField(*definition, *updated);
}

void StudentInfoUseCases::ensureStudentExists(std::string_view studentId)
{
    if (!school_index_->getStudent(studentId)) {
        throw ApplicationError(ApplicationErrorCode::NotFound, "student not found");
    }
}

}  // namespace schoolmanager::application
