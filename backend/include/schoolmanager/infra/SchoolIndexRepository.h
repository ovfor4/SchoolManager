#pragma once

#include "schoolmanager/domain/Models.h"
#include "schoolmanager/infra/LruSqlitePool.h"
#include "schoolmanager/infra/StoragePaths.h"

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
    void touchStudent(std::string_view studentId, std::int64_t updatedAt);

  private:
    std::shared_ptr<LruSqlitePool> pool_;
    StoragePaths paths_;
};

}  // namespace schoolmanager::infra
