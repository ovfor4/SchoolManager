#pragma once

#include <string_view>

namespace schoolmanager::config {

inline constexpr std::string_view schoolWebSocketRoomId = "__school_index__";
inline constexpr std::string_view webSocketStudentIdParameter = "student_id";
inline constexpr std::string_view webSocketScopeParameter = "scope";
inline constexpr std::string_view webSocketSchoolScope = "school";

}  // namespace schoolmanager::config
