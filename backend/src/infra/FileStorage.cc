#include "schoolmanager/infra/FileStorage.h"

#include "schoolmanager/config/Constants.h"

#include <stdexcept>
#include <system_error>
#include <utility>

namespace schoolmanager::infra {

FileStorage::FileStorage(StoragePaths paths) : paths_(std::move(paths))
{
}

std::filesystem::path FileStorage::rootDir(const domain::FileContext& context) const
{
    if (context.type == config::studentUploadsContextType) {
        return paths_.studentUploadsDir(context.id);
    }
    if (context.type == config::globalTemplatesContextType &&
        context.id == config::defaultGlobalTemplatesContextId) {
        return paths_.globalTemplateLibraryDir(context.id);
    }
    throw std::runtime_error("unsupported file context type");
}

std::filesystem::path FileStorage::activeFilePath(const domain::FileContext& context,
                                                  std::string_view storageName) const
{
    return rootDir(context) / std::string(storageName);
}

std::filesystem::path FileStorage::tempFilePath(const domain::FileContext& context,
                                                std::string_view storageName) const
{
    auto path = activeFilePath(context, storageName);
    path += ".tmp";
    return path;
}

std::filesystem::path FileStorage::trashEntryDir(const domain::FileContext& context,
                                                 std::string_view trashEntryId,
                                                 std::string_view rootName) const
{
    return rootDir(context) / std::string(config::trashFolderName) /
           (std::string(trashEntryId) + "-" + sanitizeFileName(rootName));
}

void FileStorage::ensureRoot(const domain::FileContext& context) const
{
    std::filesystem::create_directories(rootDir(context));
    std::filesystem::create_directories(rootDir(context) / std::string(config::trashFolderName));
}

void FileStorage::moveFile(const std::filesystem::path& source,
                           const std::filesystem::path& destination) const
{
    std::filesystem::create_directories(destination.parent_path());
    std::filesystem::rename(source, destination);
}

void FileStorage::removeFileIfExists(const std::filesystem::path& path) const
{
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

void FileStorage::removeTreeIfExists(const std::filesystem::path& path) const
{
    std::error_code error;
    std::filesystem::remove_all(path, error);
    if (error) {
        throw std::runtime_error("failed to remove file tree: " + error.message());
    }
}

}  // namespace schoolmanager::infra
