#pragma once

#include "schoolmanager/domain/Models.h"
#include "schoolmanager/infra/SchoolIndexRepository.h"
#include "schoolmanager/infra/StudentDataRepository.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace schoolmanager::application {

class StudentUseCases {
  public:
    StudentUseCases(std::shared_ptr<infra::SchoolIndexRepository> schoolIndex,
                    std::shared_ptr<infra::StudentDataRepository> studentData);

    std::vector<domain::StudentSummary> listStudents();
    domain::StudentSummary createStudent(std::string displayName);
    domain::StudentDetail getStudentDetail(std::string_view studentId);
    domain::StudentSummary updateStudentDisplayName(std::string_view studentId,
                                                    std::string displayName);
    void deleteStudent(std::string_view studentId);
    bool studentExists(std::string_view studentId);

  private:
    std::shared_ptr<infra::SchoolIndexRepository> school_index_;
    std::shared_ptr<infra::StudentDataRepository> student_data_;
};

}  // namespace schoolmanager::application
