#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace schoolmanager::config {

inline constexpr std::string_view envDataRoot = "SM_DATA_ROOT";
inline constexpr std::string_view envHost = "SM_HOST";
inline constexpr std::string_view envPort = "SM_PORT";
inline constexpr std::string_view envDbPoolSize = "SM_DB_POOL_SIZE";
inline constexpr std::string_view envOpenApiPath = "SM_OPENAPI_PATH";

inline constexpr std::string_view defaultDataRoot = "./runtime-data";
inline constexpr std::string_view defaultBackendHost = "127.0.0.1";
inline constexpr std::uint16_t defaultBackendPort = 8080;
inline constexpr std::size_t defaultDbPoolSize = 64;
inline constexpr std::string_view defaultOpenApiPath = "backend/openapi/openapi.json";
inline constexpr std::size_t defaultHttpThreadCount = 4;

inline constexpr std::string_view schoolWebSocketRoomId = "__school_index__";
inline constexpr std::string_view webSocketStudentIdParameter = "student_id";
inline constexpr std::string_view webSocketScopeParameter = "scope";
inline constexpr std::string_view webSocketSchoolScope = "school";

}  // namespace schoolmanager::config
