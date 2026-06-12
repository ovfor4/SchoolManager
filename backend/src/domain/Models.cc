#include "schoolmanager/domain/Models.h"

#include <chrono>
#include <cctype>
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

bool isAllowedStudentInfoValueType(std::string_view valueType)
{
    return valueType == "INTEGER" || valueType == "STRING" || valueType == "DATE";
}

bool isIntegerValue(std::string_view value)
{
    if (value.empty()) {
        return true;
    }

    std::size_t offset = 0;
    if (value[0] == '-' || value[0] == '+') {
        if (value.size() == 1) {
            return false;
        }
        offset = 1;
    }

    for (std::size_t index = offset; index < value.size(); ++index) {
        if (!std::isdigit(static_cast<unsigned char>(value[index]))) {
            return false;
        }
    }
    return true;
}

int parseTwoDigits(std::string_view value, std::size_t offset)
{
    return (value[offset] - '0') * 10 + (value[offset + 1] - '0');
}

int parseFourDigits(std::string_view value)
{
    return (value[0] - '0') * 1000 + (value[1] - '0') * 100 +
           (value[2] - '0') * 10 + (value[3] - '0');
}

bool isLeapYear(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int daysInMonth(int year, int month)
{
    switch (month) {
    case 2:
        return isLeapYear(year) ? 29 : 28;
    case 4:
    case 6:
    case 9:
    case 11:
        return 30;
    default:
        return 31;
    }
}

bool isDateValue(std::string_view value)
{
    if (value.empty()) {
        return true;
    }
    if (value.size() != 10 || value[4] != '-' || value[7] != '-') {
        return false;
    }
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (index == 4 || index == 7) {
            continue;
        }
        if (!std::isdigit(static_cast<unsigned char>(value[index]))) {
            return false;
        }
    }

    const int year = parseFourDigits(value);
    const int month = parseTwoDigits(value, 5);
    const int day = parseTwoDigits(value, 8);
    if (year < 1 || month < 1 || month > 12 || day < 1) {
        return false;
    }
    return day <= daysInMonth(year, month);
}

bool isValidStudentInfoValue(std::string_view valueType, std::string_view value)
{
    if (valueType == "INTEGER") {
        return isIntegerValue(value);
    }
    if (valueType == "DATE") {
        return isDateValue(value);
    }
    return valueType == "STRING";
}

}  // namespace schoolmanager::domain
