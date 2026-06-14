#include "schoolmanager/adapters/BroadcastHub.h"
#include "schoolmanager/adapters/FileManagerController.h"
#include "schoolmanager/adapters/GradesController.h"
#include "schoolmanager/adapters/RealtimePublisher.h"
#include "schoolmanager/adapters/StudentInfoController.h"
#include "schoolmanager/adapters/StudentWebSocketController.h"
#include "schoolmanager/adapters/StudentsController.h"
#include "schoolmanager/adapters/SystemController.h"
#include "schoolmanager/application/GradeUseCases.h"
#include "schoolmanager/application/StudentInfoUseCases.h"
#include "schoolmanager/application/StudentUseCases.h"
#include "schoolmanager/config/Constants.h"
#include "schoolmanager/infra/FileManagerRepository.h"
#include "schoolmanager/infra/FileManagerService.h"
#include "schoolmanager/infra/FileStorage.h"
#include "schoolmanager/infra/LruSqlitePool.h"
#include "schoolmanager/infra/SchoolIndexRepository.h"
#include "schoolmanager/infra/StoragePaths.h"
#include "schoolmanager/infra/StudentDataRepository.h"

#include <drogon/drogon.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

std::string envOrDefault(std::string_view key, std::string_view fallback)
{
    const std::string keyString(key);
    if (const char* value = std::getenv(keyString.c_str()); value != nullptr && *value != '\0') {
        return value;
    }
    return std::string(fallback);
}

std::uint16_t portFromEnv()
{
    const auto raw =
        envOrDefault(schoolmanager::config::envPort,
                     std::to_string(schoolmanager::config::defaultBackendPort));
    const auto port = std::stoi(raw);
    if (port <= 0 || port > 65535) {
        throw std::runtime_error(std::string(schoolmanager::config::envPort) + " out of range");
    }
    return static_cast<std::uint16_t>(port);
}

std::size_t sizeFromEnv(std::string_view key, std::size_t fallback)
{
    const auto raw = envOrDefault(key, std::to_string(fallback));
    const auto size = std::stoull(raw);
    if (size == 0 || size > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error(std::string(key) + " out of range");
    }
    return static_cast<std::size_t>(size);
}

void addCorsHeaders(const drogon::HttpResponsePtr& response)
{
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, PATCH, DELETE, OPTIONS");
}

}  // namespace

int main()
{
    try {
        const auto dataRoot =
            envOrDefault(schoolmanager::config::envDataRoot,
                         schoolmanager::config::defaultDataRoot);
        const auto host =
            envOrDefault(schoolmanager::config::envHost,
                         schoolmanager::config::defaultBackendHost);
        const auto port = portFromEnv();
        const auto poolSize = static_cast<std::size_t>(
            std::stoul(envOrDefault(schoolmanager::config::envDbPoolSize,
                                    std::to_string(schoolmanager::config::defaultDbPoolSize))));
        const auto maxUploadBytes =
            sizeFromEnv(schoolmanager::config::envMaxUploadBytes,
                        schoolmanager::config::defaultMaxUploadBytes);
        const auto maxUploadMemoryBytes =
            sizeFromEnv(schoolmanager::config::envMaxUploadMemoryBytes,
                        schoolmanager::config::defaultMaxUploadMemoryBytes);
        const auto openApiPath = std::filesystem::path(
            envOrDefault(schoolmanager::config::envOpenApiPath,
                         schoolmanager::config::defaultOpenApiPath));

        schoolmanager::infra::StoragePaths paths(dataRoot);
        auto pool = std::make_shared<schoolmanager::infra::LruSqlitePool>(poolSize);
        auto schoolIndex =
            std::make_shared<schoolmanager::infra::SchoolIndexRepository>(pool, paths);
        auto studentData =
            std::make_shared<schoolmanager::infra::StudentDataRepository>(pool, paths);
        auto fileManagerRepository =
            std::make_shared<schoolmanager::infra::FileManagerRepository>(pool, paths);
        auto fileManager = std::make_shared<schoolmanager::infra::FileManagerService>(
            fileManagerRepository,
            schoolmanager::infra::FileStorage(paths),
            schoolIndex);
        auto broadcastHub = std::make_shared<schoolmanager::adapters::BroadcastHub>();
        auto realtimePublisher =
            std::make_shared<schoolmanager::adapters::RealtimePublisher>(broadcastHub);
        auto studentUseCases =
            std::make_shared<schoolmanager::application::StudentUseCases>(schoolIndex, studentData);
        auto studentInfoUseCases =
            std::make_shared<schoolmanager::application::StudentInfoUseCases>(schoolIndex,
                                                                              studentData);
        auto gradeUseCases =
            std::make_shared<schoolmanager::application::GradeUseCases>(schoolIndex, studentData);

        schoolIndex->initialize();

        drogon::app().registerPreRoutingAdvice(
            [](const drogon::HttpRequestPtr& request,
               drogon::AdviceCallback&& callback,
               drogon::AdviceChainCallback&& chainCallback) {
                if (request->method() == drogon::Options) {
                    auto response = drogon::HttpResponse::newHttpResponse();
                    response->setStatusCode(drogon::k204NoContent);
                    addCorsHeaders(response);
                    callback(response);
                    return;
                }
                chainCallback();
            });

        drogon::app().registerPreSendingAdvice(
            [](const drogon::HttpRequestPtr&, const drogon::HttpResponsePtr& response) {
                addCorsHeaders(response);
            });

        drogon::app().registerController(
            std::make_shared<schoolmanager::adapters::SystemController>(openApiPath));
        drogon::app().registerController(std::make_shared<schoolmanager::adapters::StudentsController>(
            studentUseCases,
            realtimePublisher));
        drogon::app().registerController(
            std::make_shared<schoolmanager::adapters::StudentInfoController>(
                studentInfoUseCases,
                realtimePublisher));
        drogon::app().registerController(std::make_shared<schoolmanager::adapters::GradesController>(
            gradeUseCases,
            realtimePublisher));
        drogon::app().registerController(
            std::make_shared<schoolmanager::adapters::FileManagerController>(
                fileManager,
                realtimePublisher));
        schoolmanager::adapters::StudentWebSocketController::setBroadcastHub(broadcastHub);
        schoolmanager::adapters::StudentWebSocketController::initPathRouting();

        std::cout << "SchoolManager backend listening on http://" << host << ":" << port
                  << " with data root " << std::filesystem::absolute(dataRoot) << '\n';

        drogon::app()
            .setClientMaxBodySize(maxUploadBytes)
            .setClientMaxMemoryBodySize(maxUploadMemoryBytes)
            .addListener(host, port)
            .setThreadNum(schoolmanager::config::defaultHttpThreadCount)
            .run();
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
