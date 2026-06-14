#pragma once

#include <filesystem>
#include <string>

namespace schoolmanager::infra {

class StoragePaths {
  public:
    explicit StoragePaths(std::filesystem::path root);

    void ensureRoot() const;

    const std::filesystem::path& root() const noexcept;
    std::filesystem::path schoolIndexDb() const;
    std::filesystem::path studentsRoot() const;
    std::filesystem::path studentDir(std::string_view studentId) const;
    std::filesystem::path studentDataDb(std::string_view studentId) const;
    std::filesystem::path studentUploadsDir(std::string_view studentId) const;
    std::filesystem::path globalTemplatesRoot() const;
    std::filesystem::path globalTemplateLibraryDir(std::string_view libraryId) const;

  private:
    std::filesystem::path root_;
};

std::string sanitizeFileName(std::string_view name);

}  // namespace schoolmanager::infra
