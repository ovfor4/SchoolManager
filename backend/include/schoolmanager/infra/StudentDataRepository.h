#pragma once

#include "schoolmanager/domain/Models.h"
#include "schoolmanager/infra/LruSqlitePool.h"
#include "schoolmanager/infra/StoragePaths.h"

#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace schoolmanager::infra {

class StudentDataRepository {
  public:
    StudentDataRepository(std::shared_ptr<LruSqlitePool> pool, StoragePaths paths);

    void initializeStudent(std::string_view studentId);

    std::vector<domain::StudentInfoValue> listStudentInfoValues(std::string_view studentId);
    std::optional<domain::StudentInfoValue> getStudentInfoValue(std::string_view studentId,
                                                                std::string_view fieldId);
    std::optional<domain::StudentInfoValue> patchStudentInfoValue(std::string_view studentId,
                                                                  std::string_view fieldId,
                                                                  std::string_view valueType,
                                                                  std::string value);
    bool deleteStudentInfoValue(std::string_view studentId, std::string_view fieldId);

    domain::Grade createGrade(std::string_view studentId,
                              std::string title,
                              std::string score,
                              std::string maxScore,
                              std::string occurredOn,
                              std::string notes);
    std::vector<domain::Grade> listGrades(std::string_view studentId);
    std::optional<domain::Grade> getGrade(std::string_view studentId, std::string_view gradeId);
    std::optional<domain::Grade> patchGrade(std::string_view studentId,
                                            std::string_view gradeId,
                                            const std::map<std::string, std::string>& changes);
    bool deleteGrade(std::string_view studentId, std::string_view gradeId);

    domain::StoredFile createPendingFile(std::string_view studentId,
                                         std::string originalName,
                                         std::string storedName,
                                         std::string mimeType,
                                         std::uint64_t sizeBytes);
    std::optional<domain::StoredFile> activateFile(std::string_view studentId,
                                                   std::string_view fileId);
    void deleteFileRecord(std::string_view studentId, std::string_view fileId);
    std::vector<domain::StoredFile> listFiles(std::string_view studentId);
    std::optional<domain::StoredFile> getFile(std::string_view studentId, std::string_view fileId);

    std::filesystem::path uploadsDir(std::string_view studentId) const;
    std::filesystem::path filePath(std::string_view studentId, std::string_view storedName) const;

  private:
    std::shared_ptr<LruSqlitePool> pool_;
    StoragePaths paths_;

    std::shared_ptr<SqliteConnection> acquireStudentDb(std::string_view studentId);
};

}  // namespace schoolmanager::infra
