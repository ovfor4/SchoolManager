#pragma once

#include "schoolmanager/domain/Models.h"
#include "schoolmanager/infra/StoragePaths.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace schoolmanager::infra {

class FileStorage {
  public:
    explicit FileStorage(StoragePaths paths);

    std::filesystem::path rootDir(const domain::FileContext& context) const;
    std::filesystem::path activeFilePath(const domain::FileContext& context,
                                         std::string_view storageName) const;
    std::filesystem::path tempFilePath(const domain::FileContext& context,
                                       std::string_view storageName) const;
    std::filesystem::path trashEntryDir(const domain::FileContext& context,
                                        std::string_view trashEntryId,
                                        std::string_view rootName) const;

    void ensureRoot(const domain::FileContext& context) const;
    void moveFile(const std::filesystem::path& source, const std::filesystem::path& destination) const;
    void removeFileIfExists(const std::filesystem::path& path) const;
    void removeTreeIfExists(const std::filesystem::path& path) const;

  private:
    StoragePaths paths_;
};

}  // namespace schoolmanager::infra
