#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace schoolmanager::domain {

struct StudentSummary {
    std::string id;
    std::string display_name;
    std::string folder_path;
    std::int64_t created_at{};
    std::int64_t updated_at{};
};

struct Grade {
    std::string id;
    std::string title;
    std::string score;
    std::string max_score;
    std::string occurred_on;
    std::string notes;
    std::int64_t updated_at{};
};

struct StoredFile {
    std::string id;
    std::string original_name;
    std::string stored_name;
    std::string mime_type;
    std::string status;
    std::uint64_t size_bytes{};
    std::int64_t created_at{};
    std::int64_t updated_at{};
};

struct StudentDetail {
    StudentSummary student;
    std::vector<Grade> grades;
    std::vector<StoredFile> files;
};

std::int64_t unixTimeMillis();

std::string createId();

bool isSafeId(std::string_view value);

}  // namespace schoolmanager::domain
