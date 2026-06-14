#include "schoolmanager/adapters/SystemController.h"

#include "schoolmanager/adapters/HttpJson.h"

#include <fstream>
#include <sstream>
#include <utility>

namespace schoolmanager::adapters {

namespace {

using Callback = std::function<void(const drogon::HttpResponsePtr&)>;

}  // namespace

SystemController::SystemController(std::filesystem::path openApiPath)
    : open_api_path_(std::move(openApiPath))
{
}

void SystemController::health(const drogon::HttpRequestPtr&, Callback&& callback)
{
    Json::Value body(Json::objectValue);
    body["status"] = "ok";
    body["service"] = "schoolmanager_backend";
    callback(jsonResponse(std::move(body)));
}

void SystemController::openApi(const drogon::HttpRequestPtr&, Callback&& callback)
{
    std::ifstream input(open_api_path_);
    if (!input) {
        callback(errorResponse("openapi contract file not found", drogon::k500InternalServerError));
        return;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    auto response = drogon::HttpResponse::newHttpResponse();
    response->setStatusCode(drogon::k200OK);
    response->setContentTypeString("application/json");
    response->setBody(buffer.str());
    callback(response);
}

}  // namespace schoolmanager::adapters
