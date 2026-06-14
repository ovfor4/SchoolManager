#include "schoolmanager/infra/StoragePaths.h"

#include "schoolmanager/config/Constants.h"

#include <cctype>

namespace schoolmanager::infra {

StoragePaths::StoragePaths(std::filesystem::path root)
    : root_(std::filesystem::absolute(std::move(root)).lexically_normal())
{
}

void StoragePaths::ensureRoot() const
{
    std::filesystem::create_directories(studentsRoot());
    std::filesystem::create_directories(globalTemplatesRoot());
}

const std::filesystem::path& StoragePaths::root() const noexcept
{
    return root_;
}

std::filesystem::path StoragePaths::schoolIndexDb() const
{
    return root_ / std::string(config::schoolIndexDbFileName);
}

std::filesystem::path StoragePaths::studentsRoot() const
{
    return root_ / std::string(config::studentsFolderName);
}

std::filesystem::path StoragePaths::studentDir(std::string_view studentId) const
{
    return studentsRoot() / std::string(studentId);
}

std::filesystem::path StoragePaths::studentDataDb(std::string_view studentId) const
{
    return studentDir(studentId) / std::string(config::studentDataDbFileName);
}

std::filesystem::path StoragePaths::studentUploadsDir(std::string_view studentId) const
{
    return studentDir(studentId) / std::string(config::uploadsFolderName);
}

std::filesystem::path StoragePaths::globalTemplatesRoot() const
{
    return root_ / std::string(config::globalTemplatesFolderName);
}

std::filesystem::path StoragePaths::globalTemplateLibraryDir(std::string_view libraryId) const
{
    return globalTemplatesRoot() / std::string(libraryId);
}

std::string sanitizeFileName(std::string_view name)
{
    std::string result;
    result.reserve(name.size());
    for (const unsigned char ch : name) {
        if (std::isalnum(ch) || ch == '.' || ch == '-' || ch == '_') {
            result.push_back(static_cast<char>(ch));
        } else if (ch == ' ') {
            result.push_back('_');
        }
    }

    if (result.empty() || result == "." || result == "..") {
        return "uploaded_file";
    }
    if (result.size() > 160) {
        result.resize(160);
    }
    return result;
}

}  // namespace schoolmanager::infra
