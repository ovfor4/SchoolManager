cmake_minimum_required(VERSION 3.28)

if(POLICY CMP0153)
    cmake_policy(SET CMP0153 OLD)
endif()

find_package(Drogon REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(ZLIB REQUIRED)
pkg_check_modules(CMARK_GFM REQUIRED IMPORTED_TARGET libcmark-gfm)
find_library(HPDF_LIBRARY REQUIRED NAMES hpdf)

add_executable(schoolmanager_backend
    src/main.cc
    src/application/DocumentFormats.cc
    src/application/FileTemplateUseCases.cc
    src/application/GradeUseCases.cc
    src/application/StudentInfoFields.cc
    src/application/StudentInfoUseCases.cc
    src/application/StudentUseCases.cc
    src/application/TemplateProcessor.cc
    src/application/TemplateTextRenderer.cc
    src/domain/Models.cc
    src/infra/FileManagerRepository.cc
    src/infra/FileManagerService.cc
    src/infra/FileStorage.cc
    src/infra/LruSqlitePool.cc
    src/infra/SchoolIndexRepository.cc
    src/infra/SqliteConnection.cc
    src/infra/StoragePaths.cc
    src/infra/StudentDataRepository.cc
    src/infra/ZipArchiveWriter.cc
    src/adapters/BroadcastHub.cc
    src/adapters/FileManagerController.cc
    src/adapters/FileTemplateController.cc
    src/adapters/GradesController.cc
    src/adapters/HttpJson.cc
    src/adapters/JsonHelpers.cc
    src/adapters/RealtimePublisher.cc
    src/adapters/StudentInfoController.cc
    src/adapters/StudentWebSocketController.cc
    src/adapters/StudentsController.cc
    src/adapters/SystemController.cc
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
        PkgConfig::CMARK_GFM
        SQLite3_lib
        ${HPDF_LIBRARY}
        ZLIB::ZLIB
)
