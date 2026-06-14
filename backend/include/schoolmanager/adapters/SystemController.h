#pragma once

#include <drogon/HttpController.h>

#include <filesystem>
#include <functional>

namespace schoolmanager::adapters {

class SystemController : public drogon::HttpController<SystemController, false> {
  public:
    explicit SystemController(std::filesystem::path openApiPath);

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(SystemController::health, "/api/health", drogon::Get);
    ADD_METHOD_TO(SystemController::openApi, "/api/openapi.json", drogon::Get);
    METHOD_LIST_END

    void health(const drogon::HttpRequestPtr& request,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void openApi(const drogon::HttpRequestPtr& request,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback);

  private:
    std::filesystem::path open_api_path_;
};

}  // namespace schoolmanager::adapters
