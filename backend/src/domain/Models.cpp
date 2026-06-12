#include "schoolmanager/domain/Models.h"

#include <chrono>
#include <random>
#include <sstream>

namespace schoolmanager::domain {

std::int64_t unixTimeMillis()
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

std::string createId()
{
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<std::uint64_t> dist;

    std::ostringstream out;
    out << std::hex;
    out.width(16);
    out.fill('0');
    out << dist(rng);
    out.width(16);
    out.fill('0');
    out << dist(rng);
    return out.str();
}

bool isSafeId(std::string_view value)
{
    if (value.empty() || value.size() > 64) {
        return false;
    }

    for (const char c : value) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '-' || c == '_';
        if (!ok) {
            return false;
        }
    }
    return true;
}

}  // namespace schoolmanager::domain
