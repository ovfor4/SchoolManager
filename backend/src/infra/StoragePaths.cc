#include "schoolmanager/infra/StoragePaths.h"

#include <cctype>

namespace schoolmanager::infra {

StoragePaths::StoragePaths(std::filesystem::path root)
    : root_(std::filesystem::absolute(std::move(root)).lexically_normal())
{
}

void StoragePaths::ensureRoot() const
{
    std::filesystem::create_directories(studentsRoot());
}

const std::filesystem::path& StoragePaths::root() const noexcept
{
    return root_;
}

std::filesystem::path StoragePaths::schoolIndexDb() const
{
    return root_ / "school_index.db";
}

std::filesystem::path StoragePaths::studentsRoot() const
{
    return root_ / "students";
}

std::filesystem::path StoragePaths::studentDir(std::string_view studentId) const
{
    return studentsRoot() / std::string(studentId);
}

std::filesystem::path StoragePaths::studentDataDb(std::string_view studentId) const
{
    return studentDir(studentId) / "data.db";
}

std::filesystem::path StoragePaths::studentUploadsDir(std::string_view studentId) const
{
    return studentDir(studentId) / "uploads";
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
