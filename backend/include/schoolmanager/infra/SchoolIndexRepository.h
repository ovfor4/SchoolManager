#pragma once

#include "schoolmanager/domain/Models.h"
#include "schoolmanager/infra/LruSqlitePool.h"
#include "schoolmanager/infra/StoragePaths.h"

#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace schoolmanager::infra {

class SchoolIndexRepository {
  public:
    SchoolIndexRepository(std::shared_ptr<LruSqlitePool> pool, StoragePaths paths);

    void initialize();
    domain::StudentSummary createStudent(std::string displayName);
    std::vector<domain::StudentSummary> listStudents();
    std::optional<domain::StudentSummary> getStudent(std::string_view studentId);
    std::optional<domain::StudentSummary> updateStudentDisplayName(std::string_view studentId,
                                                                   std::string displayName);
    bool deleteStudent(std::string_view studentId);
    void touchStudent(std::string_view studentId, std::int64_t updatedAt);

    domain::StudentInfoDefinition createStudentInfoDefinition(std::string name,
                                                              std::string displayName,
                                                              std::string valueType);
    std::vector<domain::StudentInfoDefinition> listStudentInfoDefinitions();
    std::optional<domain::StudentInfoDefinition> getStudentInfoDefinition(
        std::string_view fieldId);
    std::optional<domain::StudentInfoDefinition> patchStudentInfoDefinition(
        std::string_view fieldId,
        const std::map<std::string, std::string>& changes);
    bool deleteStudentInfoDefinition(std::string_view fieldId);

  private:
    std::shared_ptr<LruSqlitePool> pool_;
    StoragePaths paths_;
};

}  // namespace schoolmanager::infra
