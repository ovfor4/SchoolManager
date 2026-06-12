#pragma once

#include <sqlite3.h>

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace schoolmanager::infra {

class Statement {
  public:
    Statement(sqlite3* db, std::string_view sql);
    ~Statement();

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;

    void bindText(int index, std::string_view value);
    void bindInt64(int index, std::int64_t value);
    void bindNull(int index);

    bool step();
    void executeDone();

    std::string columnText(int index) const;
    std::optional<std::string> columnOptionalText(int index) const;
    std::int64_t columnInt64(int index) const;

  private:
    sqlite3* db_{};
    sqlite3_stmt* stmt_{};

    void finalize() noexcept;
};

class SqliteConnection {
  public:
    explicit SqliteConnection(std::filesystem::path path);
    ~SqliteConnection();

    SqliteConnection(const SqliteConnection&) = delete;
    SqliteConnection& operator=(const SqliteConnection&) = delete;

    std::unique_lock<std::mutex> lock();
    Statement prepare(std::string_view sql);
    void executeUnlocked(std::string_view sql);
    void execute(std::string_view sql);

    sqlite3* raw() const noexcept;
    const std::filesystem::path& path() const noexcept;

  private:
    std::filesystem::path path_;
    sqlite3* db_{};
    std::mutex mutex_;
};

}  // namespace schoolmanager::infra
