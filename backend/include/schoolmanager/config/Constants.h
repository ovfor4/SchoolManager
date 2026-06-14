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
inline constexpr std::string_view envMaxUploadBytes = "SM_MAX_UPLOAD_BYTES";
inline constexpr std::string_view envMaxUploadMemoryBytes = "SM_MAX_UPLOAD_MEMORY_BYTES";

inline constexpr std::string_view defaultDataRoot = "./runtime-data";
inline constexpr std::string_view defaultBackendHost = "127.0.0.1";
inline constexpr std::uint16_t defaultBackendPort = 8080;
inline constexpr std::size_t defaultDbPoolSize = 64;
inline constexpr std::string_view defaultOpenApiPath = "backend/openapi/openapi.json";
inline constexpr std::size_t defaultHttpThreadCount = 4;
inline constexpr std::size_t defaultMaxUploadBytes = 1024ULL * 1024ULL * 1024ULL;
inline constexpr std::size_t defaultMaxUploadMemoryBytes = 1024ULL * 1024ULL;

inline constexpr std::string_view schoolWebSocketRoomId = "__school_index__";
inline constexpr std::string_view webSocketStudentIdParameter = "student_id";
inline constexpr std::string_view webSocketScopeParameter = "scope";
inline constexpr std::string_view webSocketSchoolScope = "school";

inline constexpr std::string_view studentUploadsContextType = "student_uploads";
inline constexpr std::string_view globalTemplatesContextType = "global_templates";
inline constexpr std::string_view defaultGlobalTemplatesContextId = "default";

inline constexpr std::string_view fileEntryKindFile = "file";
inline constexpr std::string_view fileEntryKindFolder = "folder";

inline constexpr std::string_view fileEntryStatusPending = "pending";
inline constexpr std::string_view fileEntryStatusActive = "active";
inline constexpr std::string_view fileEntryStatusTrashed = "trashed";

inline constexpr std::string_view trashFolderName = ".trash";
inline constexpr std::string_view studentsFolderName = "students";
inline constexpr std::string_view uploadsFolderName = "uploads";
inline constexpr std::string_view globalTemplatesFolderName = "global_templates";
inline constexpr std::string_view studentDataDbFileName = "data.db";
inline constexpr std::string_view schoolIndexDbFileName = "school_index.db";

inline constexpr std::string_view octetStreamMimeType = "application/octet-stream";
inline constexpr std::string_view plainTextMimeType = "text/plain";
inline constexpr std::string_view zipMimeType = "application/zip";
inline constexpr std::string_view generatedTemplatesZipFileName = "generated-files.zip";
inline constexpr std::uint16_t zipCompressionMethodDeflate = 8;
inline constexpr int zipCompressionLevel = 6;
inline constexpr std::string_view templateReservedStudentNameVariable = "__name";

inline constexpr std::string_view fileManagerWebSocketResource = "file_manager";
inline constexpr std::string_view fileManagerChangedMessageType = "file_manager.changed";
inline constexpr std::string_view fileManagerEntryCreatedAction = "entry.created";
inline constexpr std::string_view fileManagerEntryUpdatedAction = "entry.updated";
inline constexpr std::string_view fileManagerEntryTrashedAction = "entry.trashed";
inline constexpr std::string_view fileManagerTrashRestoredAction = "trash.restored";
inline constexpr std::string_view fileManagerTrashDeletedAction = "trash.deleted";

}  // namespace schoolmanager::config
