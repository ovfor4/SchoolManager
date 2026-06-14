#pragma once

#include "schoolmanager/domain/Models.h"
#include "schoolmanager/infra/SchoolIndexRepository.h"
#include "schoolmanager/infra/StudentDataRepository.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace schoolmanager::application {

struct GradeCreateInput {
    std::string title;
    std::string score;
    std::string max_score;
    std::string occurred_on;
    std::string notes;
};

class GradeUseCases {
  public:
    GradeUseCases(std::shared_ptr<infra::SchoolIndexRepository> schoolIndex,
                  std::shared_ptr<infra::StudentDataRepository> studentData);

    std::vector<domain::Grade> listGrades(std::string_view studentId);
    domain::Grade createGrade(std::string_view studentId, GradeCreateInput input);
    domain::Grade patchGrade(std::string_view studentId,
                             std::string_view gradeId,
                             const std::map<std::string, std::string>& changes);
    void deleteGrade(std::string_view studentId, std::string_view gradeId);

  private:
    void ensureStudentExists(std::string_view studentId);

    std::shared_ptr<infra::SchoolIndexRepository> school_index_;
    std::shared_ptr<infra::StudentDataRepository> student_data_;
};

}  // namespace schoolmanager::application
