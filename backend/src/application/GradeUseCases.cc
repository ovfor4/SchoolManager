#include "schoolmanager/application/GradeUseCases.h"

#include "schoolmanager/application/ApplicationError.h"

#include <utility>

namespace schoolmanager::application {

GradeUseCases::GradeUseCases(std::shared_ptr<infra::SchoolIndexRepository> schoolIndex,
                             std::shared_ptr<infra::StudentDataRepository> studentData)
    : school_index_(std::move(schoolIndex)), student_data_(std::move(studentData))
{
}

std::vector<domain::Grade> GradeUseCases::listGrades(std::string_view studentId)
{
    ensureStudentExists(studentId);
    return student_data_->listGrades(studentId);
}

domain::Grade GradeUseCases::createGrade(std::string_view studentId, GradeCreateInput input)
{
    ensureStudentExists(studentId);
    auto grade = student_data_->createGrade(studentId,
                                            std::move(input.title),
                                            std::move(input.score),
                                            std::move(input.max_score),
                                            std::move(input.occurred_on),
                                            std::move(input.notes));
    school_index_->touchStudent(studentId, grade.updated_at);
    return grade;
}

domain::Grade GradeUseCases::patchGrade(
    std::string_view studentId,
    std::string_view gradeId,
    const std::map<std::string, std::string>& changes)
{
    if (changes.empty()) {
        throw ApplicationError(ApplicationErrorCode::BadRequest,
                               "no supported grade fields provided");
    }

    ensureStudentExists(studentId);
    const auto updated = student_data_->patchGrade(studentId, gradeId, changes);
    if (!updated) {
        throw ApplicationError(ApplicationErrorCode::NotFound, "grade not found");
    }

    school_index_->touchStudent(studentId, updated->updated_at);
    return *updated;
}

void GradeUseCases::deleteGrade(std::string_view studentId, std::string_view gradeId)
{
    ensureStudentExists(studentId);
    if (!student_data_->deleteGrade(studentId, gradeId)) {
        throw ApplicationError(ApplicationErrorCode::NotFound, "grade not found");
    }

    school_index_->touchStudent(studentId, domain::unixTimeMillis());
}

void GradeUseCases::ensureStudentExists(std::string_view studentId)
{
    if (!school_index_->getStudent(studentId)) {
        throw ApplicationError(ApplicationErrorCode::NotFound, "student not found");
    }
}

}  // namespace schoolmanager::application
