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

class StudentInfoUseCases {
  public:
    StudentInfoUseCases(std::shared_ptr<infra::SchoolIndexRepository> schoolIndex,
                        std::shared_ptr<infra::StudentDataRepository> studentData);

    std::vector<domain::StudentInfoDefinition> listDefinitions();
    domain::StudentInfoDefinition createDefinition(std::string name,
                                                   std::string displayName,
                                                   std::string valueType);
    domain::StudentInfoDefinition patchDefinition(
        std::string_view fieldId,
        const std::map<std::string, std::string>& changes);
    void deleteDefinition(std::string_view fieldId);

    std::vector<domain::StudentInfoField> listStudentFields(std::string_view studentId);
    domain::StudentInfoField patchStudentValue(std::string_view studentId,
                                               std::string_view fieldId,
                                               std::string value);

  private:
    void ensureStudentExists(std::string_view studentId);

    std::shared_ptr<infra::SchoolIndexRepository> school_index_;
    std::shared_ptr<infra::StudentDataRepository> student_data_;
};

}  // namespace schoolmanager::application
