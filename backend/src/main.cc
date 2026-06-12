#include "schoolmanager/adapters/ApiController.h"
#include "schoolmanager/adapters/BroadcastHub.h"
#include "schoolmanager/adapters/StudentWebSocketController.h"
#include "schoolmanager/config/Constants.h"
#include "schoolmanager/infra/LruSqlitePool.h"
#include "schoolmanager/infra/SchoolIndexRepository.h"
#include "schoolmanager/infra/StoragePaths.h"
#include "schoolmanager/infra/StudentDataRepository.h"

#include <drogon/drogon.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
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
        const auto openApiPath = std::filesystem::path(
            envOrDefault(schoolmanager::config::envOpenApiPath,
                         schoolmanager::config::defaultOpenApiPath));

        schoolmanager::infra::StoragePaths paths(dataRoot);
        auto pool = std::make_shared<schoolmanager::infra::LruSqlitePool>(poolSize);
        auto schoolIndex =
            std::make_shared<schoolmanager::infra::SchoolIndexRepository>(pool, paths);
        auto studentData =
            std::make_shared<schoolmanager::infra::StudentDataRepository>(pool, paths);
        auto broadcastHub = std::make_shared<schoolmanager::adapters::BroadcastHub>();

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
            std::make_shared<schoolmanager::adapters::ApiController>(
                schoolIndex, studentData, broadcastHub, openApiPath));
        schoolmanager::adapters::StudentWebSocketController::setBroadcastHub(broadcastHub);
        schoolmanager::adapters::StudentWebSocketController::initPathRouting();

        std::cout << "SchoolManager backend listening on http://" << host << ":" << port
                  << " with data root " << std::filesystem::absolute(dataRoot) << '\n';

        drogon::app()
            .addListener(host, port)
            .setThreadNum(schoolmanager::config::defaultHttpThreadCount)
            .run();
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
