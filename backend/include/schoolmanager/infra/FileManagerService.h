#pragma once

#include "schoolmanager/domain/Models.h"
#include "schoolmanager/infra/FileManagerRepository.h"
#include "schoolmanager/infra/FileStorage.h"
#include "schoolmanager/infra/SchoolIndexRepository.h"

#include <filesystem>
#include <functional>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace schoolmanager::infra {

enum class FileManagerErrorCode {
    BadRequest,
    NotFound,
    Conflict,
};

class FileManagerError : public std::runtime_error {
  public:
    FileManagerError(FileManagerErrorCode code, const std::string& message);

    FileManagerErrorCode code() const noexcept;

  private:
    FileManagerErrorCode code_;
};

struct FileUploadPayload {
    std::string file_name;
    std::string mime_type;
    std::uint64_t size_bytes{};
    std::function<void(const std::filesystem::path&)> write_temp_file;
};

struct FileDownload {
    domain::FileEntry entry;
    std::filesystem::path path;
};

class FileManagerService {
  public:
    FileManagerService(std::shared_ptr<FileManagerRepository> repository,
                       FileStorage storage,
                       std::shared_ptr<SchoolIndexRepository> schoolIndex);

    std::vector<domain::FileEntry> listEntries(const domain::FileContext& context,
                                               const std::optional<std::string>& parentId);
    std::optional<domain::FileEntry> getEntry(const domain::FileContext& context,
                                              std::string_view entryId);
    domain::FileEntry uploadFile(const domain::FileContext& context,
                                 const std::optional<std::string>& parentId,
                                 const FileUploadPayload& payload);
    domain::FileEntry createFolder(const domain::FileContext& context,
                                   const std::optional<std::string>& parentId,
                                   std::string name);
    domain::FileEntry renameEntry(const domain::FileContext& context,
                                  std::string_view entryId,
                                  std::string name);
    void moveEntryToTrash(const domain::FileContext& context, std::string_view entryId);
    FileDownload downloadFile(const domain::FileContext& context, std::string_view entryId);
    std::vector<domain::TrashEntry> listTrash(const domain::FileContext& context);
    void restoreTrashEntry(const domain::FileContext& context, std::string_view trashEntryId);
    void permanentlyDeleteTrashEntry(const domain::FileContext& context,
                                     std::string_view trashEntryId);

  private:
    std::shared_ptr<FileManagerRepository> repository_;
    FileStorage storage_;
    std::shared_ptr<SchoolIndexRepository> school_index_;

    void ensureContext(const domain::FileContext& context);
    void ensureParent(const domain::FileContext& context,
                      const std::optional<std::string>& parentId);
    void ensureAvailableName(const domain::FileContext& context,
                             const std::optional<std::string>& parentId,
                             std::string_view name,
                             const std::optional<std::string>& excludedEntryId = std::nullopt);
    void touchContext(const domain::FileContext& context, std::int64_t updatedAt);
};

}  // namespace schoolmanager::infra
