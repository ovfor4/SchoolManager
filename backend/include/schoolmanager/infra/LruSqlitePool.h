#pragma once

#include "schoolmanager/infra/SqliteConnection.h"

#include <filesystem>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace schoolmanager::infra {

class LruSqlitePool {
  public:
    explicit LruSqlitePool(std::size_t maxOpenConnections);

    std::shared_ptr<SqliteConnection> acquire(const std::filesystem::path& path);
    std::size_t size() const;

  private:
    struct Entry {
        std::shared_ptr<SqliteConnection> connection;
        std::list<std::string>::iterator lru_it;
    };

    std::size_t max_open_connections_;
    mutable std::mutex mutex_;
    std::list<std::string> lru_;
    std::unordered_map<std::string, Entry> entries_;
};

}  // namespace schoolmanager::infra
