cmake_minimum_required(VERSION 3.28)

if(POLICY CMP0153)
    cmake_policy(SET CMP0153 OLD)
endif()

find_package(Drogon REQUIRED)

add_executable(schoolmanager_backend
    src/main.cpp
    src/domain/Models.cpp
    src/infra/LruSqlitePool.cpp
    src/infra/SchoolIndexRepository.cpp
    src/infra/SqliteConnection.cpp
    src/infra/StoragePaths.cpp
    src/infra/StudentDataRepository.cpp
    src/adapters/ApiController.cpp
    src/adapters/BroadcastHub.cpp
    src/adapters/JsonHelpers.cpp
    src/adapters/StudentWebSocketController.cpp
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
