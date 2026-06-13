#pragma once

#include "schoolmanager/domain/Models.h"
#include "schoolmanager/infra/LruSqlitePool.h"
#include "schoolmanager/infra/StoragePaths.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace schoolmanager::infra {

class FileManagerRepository {
  public:
    FileManagerRepository(std::shared_ptr<LruSqlitePool> pool, StoragePaths paths);

    void initializeContext(const domain::FileContext& context);

    std::vector<domain::FileEntry> listEntries(const domain::FileContext& context,
                                               const std::optional<std::string>& parentId);
    std::optional<domain::FileEntry> getEntry(const domain::FileContext& context,
                                              std::string_view entryId);
    std::optional<domain::FileEntry> getActiveFolder(const domain::FileContext& context,
                                                     std::string_view entryId);
    std::vector<domain::FileEntry> listSubtree(const domain::FileContext& context,
                                               std::string_view rootEntryId);

    bool activeSiblingNameExists(const domain::FileContext& context,
                                 const std::optional<std::string>& parentId,
                                 std::string_view name,
                                 const std::optional<std::string>& excludedEntryId);

    domain::FileEntry createFolder(const domain::FileContext& context,
                                   const std::optional<std::string>& parentId,
                                   std::string name);
    domain::FileEntry createPendingFile(const domain::FileContext& context,
                                        const std::optional<std::string>& parentId,
                                        std::string name,
                                        std::string storageName,
                                        std::string mimeType,
                                        std::uint64_t sizeBytes);
    std::optional<domain::FileEntry> activateFile(const domain::FileContext& context,
                                                  std::string_view entryId);
    void deleteEntryRecord(const domain::FileContext& context, std::string_view entryId);
    std::optional<domain::FileEntry> renameEntry(const domain::FileContext& context,
                                                 std::string_view entryId,
                                                 std::string name);

    std::vector<domain::TrashEntry> listTrash(const domain::FileContext& context);
    std::optional<domain::TrashEntry> getTrashEntry(const domain::FileContext& context,
                                                    std::string_view trashEntryId);
    std::vector<domain::FileEntry> listTrashEntries(const domain::FileContext& context,
                                                    std::string_view trashEntryId);
    void markSubtreeTrashed(const domain::FileContext& context,
                            const domain::TrashEntry& trashEntry,
                            std::int64_t trashedAt);
    void restoreTrashEntry(const domain::FileContext& context,
                           std::string_view trashEntryId,
                           std::int64_t restoredAt);
    void permanentlyDeleteTrashEntry(const domain::FileContext& context,
                                     std::string_view trashEntryId);

  private:
    std::shared_ptr<LruSqlitePool> pool_;
    StoragePaths paths_;

    std::shared_ptr<SqliteConnection> acquireContextDb(const domain::FileContext& context);
    void migrateLegacyStudentFilesUnlocked(SqliteConnection& db, const domain::FileContext& context);
};

}  // namespace schoolmanager::infra
