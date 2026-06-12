#include "schoolmanager/infra/StudentDataRepository.h"

#include <sstream>
#include <stdexcept>

namespace schoolmanager::infra {

namespace {

domain::Grade readGrade(Statement& stmt)
{
    return domain::Grade{
        .id = stmt.columnText(0),
        .title = stmt.columnText(1),
        .score = stmt.columnText(2),
        .max_score = stmt.columnText(3),
        .occurred_on = stmt.columnText(4),
        .notes = stmt.columnText(5),
        .updated_at = stmt.columnInt64(6),
    };
}

domain::StoredFile readFile(Statement& stmt)
{
    return domain::StoredFile{
        .id = stmt.columnText(0),
        .original_name = stmt.columnText(1),
        .stored_name = stmt.columnText(2),
        .mime_type = stmt.columnText(3),
        .status = stmt.columnText(4),
        .size_bytes = static_cast<std::uint64_t>(stmt.columnInt64(5)),
        .created_at = stmt.columnInt64(6),
        .updated_at = stmt.columnInt64(7),
    };
}

bool isAllowedGradeField(const std::string& field)
{
    return field == "title" || field == "score" || field == "max_score" ||
           field == "occurred_on" || field == "notes";
}

}  // namespace

StudentDataRepository::StudentDataRepository(std::shared_ptr<LruSqlitePool> pool,
                                             StoragePaths paths)
    : pool_(std::move(pool)), paths_(std::move(paths))
{
}

void StudentDataRepository::initializeStudent(std::string_view studentId)
{
    if (!domain::isSafeId(studentId)) {
        throw std::runtime_error("invalid student id");
    }

    std::filesystem::create_directories(paths_.studentUploadsDir(studentId));
    auto db = acquireStudentDb(studentId);
    auto guard = db->lock();
    db->executeUnlocked(R"SQL(
        CREATE TABLE IF NOT EXISTS grades (
            id TEXT PRIMARY KEY,
            title TEXT NOT NULL,
            score TEXT NOT NULL,
            max_score TEXT NOT NULL DEFAULT '',
            occurred_on TEXT NOT NULL DEFAULT '',
            notes TEXT NOT NULL DEFAULT '',
            updated_at INTEGER NOT NULL
        );
    )SQL");
    db->executeUnlocked(R"SQL(
        CREATE TABLE IF NOT EXISTS files (
            id TEXT PRIMARY KEY,
            original_name TEXT NOT NULL,
            stored_name TEXT NOT NULL,
            mime_type TEXT NOT NULL,
            status TEXT NOT NULL CHECK(status IN ('pending', 'active')),
            size_bytes INTEGER NOT NULL,
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL
        );
    )SQL");
}

domain::Grade StudentDataRepository::createGrade(std::string_view studentId,
                                                 std::string title,
                                                 std::string score,
                                                 std::string maxScore,
                                                 std::string occurredOn,
                                                 std::string notes)
{
    initializeStudent(studentId);
    if (title.empty()) {
        title = "Untitled grade";
    }

    domain::Grade grade{
        .id = domain::createId(),
        .title = std::move(title),
        .score = std::move(score),
        .max_score = std::move(maxScore),
        .occurred_on = std::move(occurredOn),
        .notes = std::move(notes),
        .updated_at = domain::unixTimeMillis(),
    };

    auto db = acquireStudentDb(studentId);
    auto guard = db->lock();
    auto stmt = db->prepare(R"SQL(
        INSERT INTO grades (id, title, score, max_score, occurred_on, notes, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?);
    )SQL");
    stmt.bindText(1, grade.id);
    stmt.bindText(2, grade.title);
    stmt.bindText(3, grade.score);
    stmt.bindText(4, grade.max_score);
    stmt.bindText(5, grade.occurred_on);
    stmt.bindText(6, grade.notes);
    stmt.bindInt64(7, grade.updated_at);
    stmt.executeDone();
    return grade;
}

std::vector<domain::Grade> StudentDataRepository::listGrades(std::string_view studentId)
{
    initializeStudent(studentId);
    auto db = acquireStudentDb(studentId);
    auto guard = db->lock();
    auto stmt = db->prepare(R"SQL(
        SELECT id, title, score, max_score, occurred_on, notes, updated_at
        FROM grades
        ORDER BY updated_at DESC;
    )SQL");

    std::vector<domain::Grade> grades;
    while (stmt.step()) {
        grades.push_back(readGrade(stmt));
    }
    return grades;
}

std::optional<domain::Grade> StudentDataRepository::getGrade(std::string_view studentId,
                                                             std::string_view gradeId)
{
    if (!domain::isSafeId(gradeId)) {
        return std::nullopt;
    }
    initializeStudent(studentId);
    auto db = acquireStudentDb(studentId);
    auto guard = db->lock();
    auto stmt = db->prepare(R"SQL(
        SELECT id, title, score, max_score, occurred_on, notes, updated_at
        FROM grades
        WHERE id = ?;
    )SQL");
    stmt.bindText(1, gradeId);
    if (!stmt.step()) {
        return std::nullopt;
    }
    return readGrade(stmt);
}

std::optional<domain::Grade> StudentDataRepository::patchGrade(
    std::string_view studentId,
    std::string_view gradeId,
    const std::map<std::string, std::string>& changes)
{
    if (!domain::isSafeId(gradeId)) {
        return std::nullopt;
    }
    if (changes.empty()) {
        return getGrade(studentId, gradeId);
    }
    for (const auto& [field, _] : changes) {
        if (!isAllowedGradeField(field)) {
            throw std::runtime_error("unsupported grade field: " + field);
        }
    }

    initializeStudent(studentId);
    const auto updatedAt = domain::unixTimeMillis();

    std::ostringstream sql;
    sql << "UPDATE grades SET ";
    bool first = true;
    for (const auto& [field, _] : changes) {
        if (!first) {
            sql << ", ";
        }
        first = false;
        sql << field << " = ?";
    }
    sql << ", updated_at = ? WHERE id = ?;";

    auto db = acquireStudentDb(studentId);
    {
        auto guard = db->lock();
        auto stmt = db->prepare(sql.str());
        int index = 1;
        for (const auto& [_, value] : changes) {
            stmt.bindText(index++, value);
        }
        stmt.bindInt64(index++, updatedAt);
        stmt.bindText(index, gradeId);
        stmt.executeDone();
        if (sqlite3_changes(db->raw()) == 0) {
            return std::nullopt;
        }
    }

    return getGrade(studentId, gradeId);
}

bool StudentDataRepository::deleteGrade(std::string_view studentId, std::string_view gradeId)
{
    if (!domain::isSafeId(gradeId)) {
        return false;
    }
    initializeStudent(studentId);
    auto db = acquireStudentDb(studentId);
    auto guard = db->lock();
    auto stmt = db->prepare("DELETE FROM grades WHERE id = ?;");
    stmt.bindText(1, gradeId);
    stmt.executeDone();
    return sqlite3_changes(db->raw()) > 0;
}

domain::StoredFile StudentDataRepository::createPendingFile(std::string_view studentId,
                                                            std::string originalName,
                                                            std::string storedName,
                                                            std::string mimeType,
                                                            std::uint64_t sizeBytes)
{
    initializeStudent(studentId);
    const auto now = domain::unixTimeMillis();
    domain::StoredFile file{
        .id = domain::createId(),
        .original_name = std::move(originalName),
        .stored_name = std::move(storedName),
        .mime_type = std::move(mimeType),
        .status = "pending",
        .size_bytes = sizeBytes,
        .created_at = now,
        .updated_at = now,
    };

    auto db = acquireStudentDb(studentId);
    auto guard = db->lock();
    auto stmt = db->prepare(R"SQL(
        INSERT INTO files (id, original_name, stored_name, mime_type, status, size_bytes, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?);
    )SQL");
    stmt.bindText(1, file.id);
    stmt.bindText(2, file.original_name);
    stmt.bindText(3, file.stored_name);
    stmt.bindText(4, file.mime_type);
    stmt.bindText(5, file.status);
    stmt.bindInt64(6, static_cast<std::int64_t>(file.size_bytes));
    stmt.bindInt64(7, file.created_at);
    stmt.bindInt64(8, file.updated_at);
    stmt.executeDone();
    return file;
}

std::optional<domain::StoredFile> StudentDataRepository::activateFile(std::string_view studentId,
                                                                      std::string_view fileId)
{
    if (!domain::isSafeId(fileId)) {
        return std::nullopt;
    }
    initializeStudent(studentId);
    const auto now = domain::unixTimeMillis();
    auto db = acquireStudentDb(studentId);
    {
        auto guard = db->lock();
        auto stmt = db->prepare("UPDATE files SET status = 'active', updated_at = ? WHERE id = ?;");
        stmt.bindInt64(1, now);
        stmt.bindText(2, fileId);
        stmt.executeDone();
        if (sqlite3_changes(db->raw()) == 0) {
            return std::nullopt;
        }
    }
    return getFile(studentId, fileId);
}

void StudentDataRepository::deleteFileRecord(std::string_view studentId, std::string_view fileId)
{
    if (!domain::isSafeId(fileId)) {
        return;
    }
    initializeStudent(studentId);
    auto db = acquireStudentDb(studentId);
    auto guard = db->lock();
    auto stmt = db->prepare("DELETE FROM files WHERE id = ?;");
    stmt.bindText(1, fileId);
    stmt.executeDone();
}

std::vector<domain::StoredFile> StudentDataRepository::listFiles(std::string_view studentId)
{
    initializeStudent(studentId);
    auto db = acquireStudentDb(studentId);
    auto guard = db->lock();
    auto stmt = db->prepare(R"SQL(
        SELECT id, original_name, stored_name, mime_type, status, size_bytes, created_at, updated_at
        FROM files
        ORDER BY created_at DESC;
    )SQL");
    std::vector<domain::StoredFile> files;
    while (stmt.step()) {
        files.push_back(readFile(stmt));
    }
    return files;
}

std::optional<domain::StoredFile> StudentDataRepository::getFile(std::string_view studentId,
                                                                 std::string_view fileId)
{
    if (!domain::isSafeId(fileId)) {
        return std::nullopt;
    }
    initializeStudent(studentId);
    auto db = acquireStudentDb(studentId);
    auto guard = db->lock();
    auto stmt = db->prepare(R"SQL(
        SELECT id, original_name, stored_name, mime_type, status, size_bytes, created_at, updated_at
        FROM files
        WHERE id = ?;
    )SQL");
    stmt.bindText(1, fileId);
    if (!stmt.step()) {
        return std::nullopt;
    }
    return readFile(stmt);
}

std::filesystem::path StudentDataRepository::uploadsDir(std::string_view studentId) const
{
    return paths_.studentUploadsDir(studentId);
}

std::filesystem::path StudentDataRepository::filePath(std::string_view studentId,
                                                      std::string_view storedName) const
{
    return paths_.studentUploadsDir(studentId) / std::string(storedName);
}

std::shared_ptr<SqliteConnection> StudentDataRepository::acquireStudentDb(std::string_view studentId)
{
    if (!domain::isSafeId(studentId)) {
        throw std::runtime_error("invalid student id");
    }
    return pool_->acquire(paths_.studentDataDb(studentId));
}

}  // namespace schoolmanager::infra
