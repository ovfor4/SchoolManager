#include "schoolmanager/infra/SchoolIndexRepository.h"

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

}  // namespace schoolmanager::infra
