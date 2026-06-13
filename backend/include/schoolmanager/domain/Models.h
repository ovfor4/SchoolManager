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

struct StudentInfoDefinition {
    std::string id;
    std::string name;
    std::string display_name;
    std::string value_type;
    std::int64_t created_at{};
    std::int64_t updated_at{};
};

struct StudentInfoValue {
    std::string field_id;
    std::string value;
    std::int64_t updated_at{};
};

struct StudentInfoField {
    std::string id;
    std::string name;
    std::string display_name;
    std::string value_type;
    std::string value;
    std::int64_t updated_at{};
};

struct FileContext {
    std::string type;
    std::string id;
};

struct FileEntry {
    std::string id;
    std::string context_type;
    std::string context_id;
    std::optional<std::string> parent_id;
    std::string kind;
    std::string name;
    std::string storage_name;
    std::string mime_type;
    std::uint64_t size_bytes{};
    std::string status;
    std::optional<std::string> trash_entry_id;
    std::int64_t created_at{};
    std::int64_t updated_at{};
    std::optional<std::int64_t> trashed_at;
};

struct TrashEntry {
    std::string id;
    std::string context_type;
    std::string context_id;
    std::string root_entry_id;
    std::optional<std::string> original_parent_id;
    std::string root_name;
    std::string root_kind;
    std::uint64_t item_count{};
    std::int64_t trashed_at{};
};

struct StudentDetail {
    StudentSummary student;
    std::vector<StudentInfoField> info_fields;
    std::vector<Grade> grades;
};

std::int64_t unixTimeMillis();

std::string createId();

bool isSafeId(std::string_view value);

bool isAllowedStudentInfoValueType(std::string_view valueType);

bool isValidStudentInfoValue(std::string_view valueType, std::string_view value);

}  // namespace schoolmanager::domain
