#include "schoolmanager/infra/SchoolIndexRepository.h"

#include <sstream>
#include <stdexcept>
#include <system_error>

namespace schoolmanager::infra {

namespace {

domain::StudentSummary readStudent(Statement& stmt)
{
    return domain::StudentSummary{
        .id = stmt.columnText(0),
        .display_name = stmt.columnText(1),
        .folder_path = stmt.columnText(2),
        .created_at = stmt.columnInt64(3),
        .updated_at = stmt.columnInt64(4),
    };
}

domain::StudentInfoDefinition readStudentInfoDefinition(Statement& stmt)
{
    return domain::StudentInfoDefinition{
        .id = stmt.columnText(0),
        .name = stmt.columnText(1),
        .display_name = stmt.columnText(2),
        .value_type = stmt.columnText(3),
        .created_at = stmt.columnInt64(4),
        .updated_at = stmt.columnInt64(5),
    };
}

bool isAllowedStudentInfoDefinitionField(const std::string& field)
{
    return field == "name" || field == "display_name" || field == "value_type";
}

void validateStudentInfoDefinitionName(std::string_view name)
{
    if (!domain::isSafeId(name)) {
        throw std::runtime_error("student info name must be a non-empty safe identifier");
    }
}

void validateStudentInfoDefinitionType(std::string_view valueType)
{
    if (!domain::isAllowedStudentInfoValueType(valueType)) {
        throw std::runtime_error("student info type must be INTEGER, STRING, or DATE");
    }
}

bool studentInfoDefinitionNameExists(SqliteConnection& db,
                                     std::string_view name,
                                     std::optional<std::string_view> excludedId = std::nullopt)
{
    if (excludedId) {
        auto stmt = db.prepare(
            "SELECT 1 FROM student_info_definitions WHERE name = ? AND id <> ? LIMIT 1;");
        stmt.bindText(1, name);
        stmt.bindText(2, *excludedId);
        return stmt.step();
    }

    auto stmt = db.prepare("SELECT 1 FROM student_info_definitions WHERE name = ? LIMIT 1;");
    stmt.bindText(1, name);
    return stmt.step();
}

std::string studentInfoDefinitionPatchValue(const domain::StudentInfoDefinition& field,
                                            const std::string& column)
{
    if (column == "name") {
        return field.name;
    }
    if (column == "display_name") {
        return field.display_name;
    }
    if (column == "value_type") {
        return field.value_type;
    }
    throw std::runtime_error("unsupported student info definition field: " + column);
}

}  // namespace

SchoolIndexRepository::SchoolIndexRepository(std::shared_ptr<LruSqlitePool> pool,
                                             StoragePaths paths)
    : pool_(std::move(pool)), paths_(std::move(paths))
{
}

void SchoolIndexRepository::initialize()
{
    paths_.ensureRoot();
    auto db = pool_->acquire(paths_.schoolIndexDb());
    db->execute(R"SQL(
        CREATE TABLE IF NOT EXISTS students (
            id TEXT PRIMARY KEY,
            display_name TEXT NOT NULL,
            folder_path TEXT NOT NULL,
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL
        );
    )SQL");
    db->execute(R"SQL(
        CREATE TABLE IF NOT EXISTS student_info_definitions (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL UNIQUE,
            display_name TEXT NOT NULL,
            value_type TEXT NOT NULL CHECK(value_type IN ('INTEGER', 'STRING', 'DATE')),
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL
        );
    )SQL");
}

domain::StudentSummary SchoolIndexRepository::createStudent(std::string displayName)
{
    if (displayName.empty()) {
        displayName = "Unnamed student";
    }

    domain::StudentSummary student;
    student.id = domain::createId();
    student.display_name = std::move(displayName);
    student.folder_path = paths_.studentDir(student.id).string();
    student.created_at = domain::unixTimeMillis();
    student.updated_at = student.created_at;

    std::filesystem::create_directories(paths_.studentUploadsDir(student.id));

    auto db = pool_->acquire(paths_.schoolIndexDb());
    auto guard = db->lock();
    auto stmt = db->prepare(R"SQL(
        INSERT INTO students (id, display_name, folder_path, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?);
    )SQL");
    stmt.bindText(1, student.id);
    stmt.bindText(2, student.display_name);
    stmt.bindText(3, student.folder_path);
    stmt.bindInt64(4, student.created_at);
    stmt.bindInt64(5, student.updated_at);
    stmt.executeDone();

    return student;
}

std::vector<domain::StudentSummary> SchoolIndexRepository::listStudents()
{
    auto db = pool_->acquire(paths_.schoolIndexDb());
    auto guard = db->lock();
    auto stmt = db->prepare(R"SQL(
        SELECT id, display_name, folder_path, created_at, updated_at
        FROM students
        ORDER BY updated_at DESC, display_name ASC;
    )SQL");

    std::vector<domain::StudentSummary> students;
    while (stmt.step()) {
        students.push_back(readStudent(stmt));
    }
    return students;
}

std::optional<domain::StudentSummary> SchoolIndexRepository::getStudent(std::string_view studentId)
{
    if (!domain::isSafeId(studentId)) {
        return std::nullopt;
    }

    auto db = pool_->acquire(paths_.schoolIndexDb());
    auto guard = db->lock();
    auto stmt = db->prepare(R"SQL(
        SELECT id, display_name, folder_path, created_at, updated_at
        FROM students
        WHERE id = ?;
    )SQL");
    stmt.bindText(1, studentId);

    if (!stmt.step()) {
        return std::nullopt;
    }
    return readStudent(stmt);
}

std::optional<domain::StudentSummary> SchoolIndexRepository::updateStudentDisplayName(
    std::string_view studentId,
    std::string displayName)
{
    if (!domain::isSafeId(studentId)) {
        return std::nullopt;
    }
    if (displayName.empty()) {
        displayName = "Unnamed student";
    }

    auto db = pool_->acquire(paths_.schoolIndexDb());
    {
        auto guard = db->lock();
        auto stmt = db->prepare(
            "UPDATE students SET display_name = ?, updated_at = ? WHERE id = ?;");
        stmt.bindText(1, displayName);
        stmt.bindInt64(2, domain::unixTimeMillis());
        stmt.bindText(3, studentId);
        stmt.executeDone();

        if (sqlite3_changes(db->raw()) == 0) {
            return std::nullopt;
        }
    }

    return getStudent(studentId);
}

bool SchoolIndexRepository::deleteStudent(std::string_view studentId)
{
    if (!domain::isSafeId(studentId)) {
        return false;
    }

    const auto studentDir = paths_.studentDir(studentId);
    auto db = pool_->acquire(paths_.schoolIndexDb());
    {
        auto guard = db->lock();
        auto stmt = db->prepare("DELETE FROM students WHERE id = ?;");
        stmt.bindText(1, studentId);
        stmt.executeDone();

        if (sqlite3_changes(db->raw()) == 0) {
            return false;
        }
    }

    std::error_code error;
    std::filesystem::remove_all(studentDir, error);
    if (error) {
        throw std::runtime_error("failed to remove student folder: " + error.message());
    }
    return true;
}

void SchoolIndexRepository::touchStudent(std::string_view studentId, std::int64_t updatedAt)
{
    auto db = pool_->acquire(paths_.schoolIndexDb());
    auto guard = db->lock();
    auto stmt = db->prepare("UPDATE students SET updated_at = ? WHERE id = ?;");
    stmt.bindInt64(1, updatedAt);
    stmt.bindText(2, studentId);
    stmt.executeDone();
}

domain::StudentInfoDefinition SchoolIndexRepository::createStudentInfoDefinition(
    std::string name,
    std::string displayName,
    std::string valueType)
{
    validateStudentInfoDefinitionName(name);
    validateStudentInfoDefinitionType(valueType);
    if (displayName.empty()) {
        displayName = name;
    }

    const auto now = domain::unixTimeMillis();
    domain::StudentInfoDefinition field{
        .id = domain::createId(),
        .name = std::move(name),
        .display_name = std::move(displayName),
        .value_type = std::move(valueType),
        .created_at = now,
        .updated_at = now,
    };

    auto db = pool_->acquire(paths_.schoolIndexDb());
    auto guard = db->lock();
    if (studentInfoDefinitionNameExists(*db, field.name)) {
        throw std::runtime_error("student info name must be unique");
    }

    auto stmt = db->prepare(R"SQL(
        INSERT INTO student_info_definitions (id, name, display_name, value_type, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?);
    )SQL");
    stmt.bindText(1, field.id);
    stmt.bindText(2, field.name);
    stmt.bindText(3, field.display_name);
    stmt.bindText(4, field.value_type);
    stmt.bindInt64(5, field.created_at);
    stmt.bindInt64(6, field.updated_at);
    stmt.executeDone();
    return field;
}

std::vector<domain::StudentInfoDefinition> SchoolIndexRepository::listStudentInfoDefinitions()
{
    auto db = pool_->acquire(paths_.schoolIndexDb());
    auto guard = db->lock();
    auto stmt = db->prepare(R"SQL(
        SELECT id, name, display_name, value_type, created_at, updated_at
        FROM student_info_definitions
        ORDER BY display_name COLLATE NOCASE ASC, name ASC;
    )SQL");

    std::vector<domain::StudentInfoDefinition> fields;
    while (stmt.step()) {
        fields.push_back(readStudentInfoDefinition(stmt));
    }
    return fields;
}

std::optional<domain::StudentInfoDefinition> SchoolIndexRepository::getStudentInfoDefinition(
    std::string_view fieldId)
{
    if (!domain::isSafeId(fieldId)) {
        return std::nullopt;
    }

    auto db = pool_->acquire(paths_.schoolIndexDb());
    auto guard = db->lock();
    auto stmt = db->prepare(R"SQL(
        SELECT id, name, display_name, value_type, created_at, updated_at
        FROM student_info_definitions
        WHERE id = ?;
    )SQL");
    stmt.bindText(1, fieldId);

    if (!stmt.step()) {
        return std::nullopt;
    }
    return readStudentInfoDefinition(stmt);
}

std::optional<domain::StudentInfoDefinition> SchoolIndexRepository::patchStudentInfoDefinition(
    std::string_view fieldId,
    const std::map<std::string, std::string>& changes)
{
    if (!domain::isSafeId(fieldId)) {
        return std::nullopt;
    }
    if (changes.empty()) {
        return getStudentInfoDefinition(fieldId);
    }
    for (const auto& [field, _] : changes) {
        if (!isAllowedStudentInfoDefinitionField(field)) {
            throw std::runtime_error("unsupported student info definition field: " + field);
        }
    }

    auto next = getStudentInfoDefinition(fieldId);
    if (!next) {
        return std::nullopt;
    }

    for (const auto& [field, value] : changes) {
        if (field == "name") {
            next->name = value;
        } else if (field == "display_name") {
            next->display_name = value;
        } else if (field == "value_type") {
            next->value_type = value;
        }
    }
    validateStudentInfoDefinitionName(next->name);
    validateStudentInfoDefinitionType(next->value_type);
    if (next->display_name.empty()) {
        next->display_name = next->name;
    }
    next->updated_at = domain::unixTimeMillis();

    std::ostringstream sql;
    sql << "UPDATE student_info_definitions SET ";
    bool first = true;
    for (const auto& [field, _] : changes) {
        if (!first) {
            sql << ", ";
        }
        first = false;
        sql << field << " = ?";
    }
    sql << ", updated_at = ? WHERE id = ?;";

    auto db = pool_->acquire(paths_.schoolIndexDb());
    {
        auto guard = db->lock();
        if (studentInfoDefinitionNameExists(*db, next->name, fieldId)) {
            throw std::runtime_error("student info name must be unique");
        }

        auto stmt = db->prepare(sql.str());
        int index = 1;
        for (const auto& [field, _] : changes) {
            stmt.bindText(index++, studentInfoDefinitionPatchValue(*next, field));
        }
        stmt.bindInt64(index++, next->updated_at);
        stmt.bindText(index, fieldId);
        stmt.executeDone();
        if (sqlite3_changes(db->raw()) == 0) {
            return std::nullopt;
        }
    }

    return getStudentInfoDefinition(fieldId);
}

bool SchoolIndexRepository::deleteStudentInfoDefinition(std::string_view fieldId)
{
    if (!domain::isSafeId(fieldId)) {
        return false;
    }

    auto db = pool_->acquire(paths_.schoolIndexDb());
    auto guard = db->lock();
    auto stmt = db->prepare("DELETE FROM student_info_definitions WHERE id = ?;");
    stmt.bindText(1, fieldId);
    stmt.executeDone();
    return sqlite3_changes(db->raw()) > 0;
}

}  // namespace schoolmanager::infra
