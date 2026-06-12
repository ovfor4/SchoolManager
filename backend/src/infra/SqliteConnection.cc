#include "schoolmanager/infra/SqliteConnection.h"

#include <stdexcept>

namespace schoolmanager::infra {

namespace {

[[noreturn]] void throwSqlite(sqlite3* db, const std::string& prefix)
{
    throw std::runtime_error(prefix + ": " + sqlite3_errmsg(db));
}

}  // namespace

Statement::Statement(sqlite3* db, std::string_view sql) : db_(db)
{
    const int rc = sqlite3_prepare_v2(
        db_, sql.data(), static_cast<int>(sql.size()), &stmt_, nullptr);
    if (rc != SQLITE_OK) {
        throwSqlite(db_, "sqlite prepare failed");
    }
}

Statement::~Statement()
{
    finalize();
}

Statement::Statement(Statement&& other) noexcept
    : db_(other.db_), stmt_(other.stmt_)
{
    other.db_ = nullptr;
    other.stmt_ = nullptr;
}

Statement& Statement::operator=(Statement&& other) noexcept
{
    if (this != &other) {
        finalize();
        db_ = other.db_;
        stmt_ = other.stmt_;
        other.db_ = nullptr;
        other.stmt_ = nullptr;
    }
    return *this;
}

void Statement::bindText(int index, std::string_view value)
{
    const int rc = sqlite3_bind_text(
        stmt_, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        throwSqlite(db_, "sqlite bind text failed");
    }
}

void Statement::bindInt64(int index, std::int64_t value)
{
    const int rc = sqlite3_bind_int64(stmt_, index, value);
    if (rc != SQLITE_OK) {
        throwSqlite(db_, "sqlite bind int64 failed");
    }
}

void Statement::bindNull(int index)
{
    const int rc = sqlite3_bind_null(stmt_, index);
    if (rc != SQLITE_OK) {
        throwSqlite(db_, "sqlite bind null failed");
    }
}

bool Statement::step()
{
    const int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) {
        return true;
    }
    if (rc == SQLITE_DONE) {
        return false;
    }
    throwSqlite(db_, "sqlite step failed");
}

void Statement::executeDone()
{
    const bool hasRow = step();
    if (hasRow) {
        throw std::runtime_error("sqlite statement unexpectedly returned rows");
    }
}

std::string Statement::columnText(int index) const
{
    const auto* text = sqlite3_column_text(stmt_, index);
    if (text == nullptr) {
        return {};
    }
    return reinterpret_cast<const char*>(text);
}

std::optional<std::string> Statement::columnOptionalText(int index) const
{
    if (sqlite3_column_type(stmt_, index) == SQLITE_NULL) {
        return std::nullopt;
    }
    return columnText(index);
}

std::int64_t Statement::columnInt64(int index) const
{
    return sqlite3_column_int64(stmt_, index);
}

void Statement::finalize() noexcept
{
    if (stmt_ != nullptr) {
        sqlite3_finalize(stmt_);
        stmt_ = nullptr;
    }
}

SqliteConnection::SqliteConnection(std::filesystem::path path) : path_(std::move(path))
{
    const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    const int rc = sqlite3_open_v2(path_.string().c_str(), &db_, flags, nullptr);
    if (rc != SQLITE_OK) {
        const std::string message = db_ != nullptr ? sqlite3_errmsg(db_) : "unknown";
        if (db_ != nullptr) {
            sqlite3_close(db_);
        }
        throw std::runtime_error("sqlite open failed for " + path_.string() + ": " + message);
    }

    executeUnlocked("PRAGMA journal_mode=WAL;");
    executeUnlocked("PRAGMA busy_timeout=5000;");
    executeUnlocked("PRAGMA foreign_keys=ON;");
    executeUnlocked("PRAGMA synchronous=NORMAL;");
}

SqliteConnection::~SqliteConnection()
{
    if (db_ != nullptr) {
        sqlite3_close(db_);
    }
}

std::unique_lock<std::mutex> SqliteConnection::lock()
{
    return std::unique_lock<std::mutex>(mutex_);
}

Statement SqliteConnection::prepare(std::string_view sql)
{
    return Statement(db_, sql);
}

void SqliteConnection::executeUnlocked(std::string_view sql)
{
    char* error = nullptr;
    const int rc = sqlite3_exec(db_, std::string(sql).c_str(), nullptr, nullptr, &error);
    if (rc != SQLITE_OK) {
        std::string message = error != nullptr ? error : sqlite3_errmsg(db_);
        sqlite3_free(error);
        throw std::runtime_error("sqlite exec failed: " + message);
    }
}

void SqliteConnection::execute(std::string_view sql)
{
    auto guard = lock();
    executeUnlocked(sql);
}

sqlite3* SqliteConnection::raw() const noexcept
{
    return db_;
}

const std::filesystem::path& SqliteConnection::path() const noexcept
{
    return path_;
}

}  // namespace schoolmanager::infra
