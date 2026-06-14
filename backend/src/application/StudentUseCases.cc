#include "schoolmanager/application/StudentUseCases.h"

#include "schoolmanager/application/ApplicationError.h"
#include "schoolmanager/application/StudentInfoFields.h"

#include <utility>

namespace schoolmanager::application {

StudentUseCases::StudentUseCases(std::shared_ptr<infra::SchoolIndexRepository> schoolIndex,
                                 std::shared_ptr<infra::StudentDataRepository> studentData)
    : school_index_(std::move(schoolIndex)), student_data_(std::move(studentData))
{
}

std::vector<domain::StudentSummary> StudentUseCases::listStudents()
{
    return school_index_->listStudents();
}

domain::StudentSummary StudentUseCases::createStudent(std::string displayName)
{
    auto student = school_index_->createStudent(std::move(displayName));
    try {
        student_data_->initializeStudent(student.id);
    } catch (...) {
        try {
            school_index_->deleteStudent(student.id);
        } catch (...) {
        }
        throw;
    }
    return student;
}

domain::StudentDetail StudentUseCases::getStudentDetail(std::string_view studentId)
{
    const auto student = school_index_->getStudent(studentId);
    if (!student) {
        throw ApplicationError(ApplicationErrorCode::NotFound, "student not found");
    }

    return domain::StudentDetail{
        .student = *student,
        .info_fields = mergeStudentInfoFields(
            school_index_->listStudentInfoDefinitions(),
            student_data_->listStudentInfoValues(studentId)),
        .grades = student_data_->listGrades(studentId),
    };
}

domain::StudentSummary StudentUseCases::updateStudentDisplayName(std::string_view studentId,
                                                                 std::string displayName)
{
    const auto updated =
        school_index_->updateStudentDisplayName(studentId, std::move(displayName));
    if (!updated) {
        throw ApplicationError(ApplicationErrorCode::NotFound, "student not found");
    }
    return *updated;
}

void StudentUseCases::deleteStudent(std::string_view studentId)
{
    if (!school_index_->deleteStudent(studentId)) {
        throw ApplicationError(ApplicationErrorCode::NotFound, "student not found");
    }
}

bool StudentUseCases::studentExists(std::string_view studentId)
{
    return school_index_->getStudent(studentId).has_value();
}

}  // namespace schoolmanager::application
