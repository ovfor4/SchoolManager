cmake_minimum_required(VERSION 3.28)

if(POLICY CMP0153)
    cmake_policy(SET CMP0153 OLD)
endif()

find_package(Drogon REQUIRED)

add_executable(schoolmanager_backend
    src/main.cc
    src/domain/Models.cc
    src/infra/FileManagerRepository.cc
    src/infra/FileManagerService.cc
    src/infra/FileStorage.cc
    src/infra/LruSqlitePool.cc
    src/infra/SchoolIndexRepository.cc
    src/infra/SqliteConnection.cc
    src/infra/StoragePaths.cc
    src/infra/StudentDataRepository.cc
    src/adapters/ApiController.cc
    src/adapters/BroadcastHub.cc
    src/adapters/JsonHelpers.cc
    src/adapters/StudentWebSocketController.cc
)

target_include_directories(schoolmanager_backend
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(schoolmanager_backend PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
target_compile_options(schoolmanager_backend PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(schoolmanager_backend
    PRIVATE
        Drogon::Drogon
        SQLite3_lib
)
