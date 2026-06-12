#include "schoolmanager/infra/LruSqlitePool.h"

#include <algorithm>

namespace schoolmanager::infra {

LruSqlitePool::LruSqlitePool(std::size_t maxOpenConnections)
    : max_open_connections_(std::max<std::size_t>(1, maxOpenConnections))
{
}

std::shared_ptr<SqliteConnection> LruSqlitePool::acquire(const std::filesystem::path& path)
{
    const auto normalized = std::filesystem::absolute(path).lexically_normal().string();
    std::filesystem::create_directories(std::filesystem::path(normalized).parent_path());

    std::lock_guard<std::mutex> guard(mutex_);
    if (auto it = entries_.find(normalized); it != entries_.end()) {
        lru_.splice(lru_.begin(), lru_, it->second.lru_it);
        return it->second.connection;
    }

    while (entries_.size() >= max_open_connections_ && !lru_.empty()) {
        const auto victim = lru_.back();
        lru_.pop_back();
        entries_.erase(victim);
    }

    lru_.push_front(normalized);
    auto connection = std::make_shared<SqliteConnection>(normalized);
    entries_.emplace(normalized, Entry{connection, lru_.begin()});
    return connection;
}

std::size_t LruSqlitePool::size() const
{
    std::lock_guard<std::mutex> guard(mutex_);
    return entries_.size();
}

}  // namespace schoolmanager::infra
