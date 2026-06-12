#include "schoolmanager/adapters/StudentWebSocketController.h"

#include "schoolmanager/config/Constants.h"
#include "schoolmanager/domain/Models.h"

#include <drogon/drogon.h>

#include <string>

namespace schoolmanager::adapters {

namespace {

std::shared_ptr<BroadcastHub>& configuredBroadcastHub()
{
    static std::shared_ptr<BroadcastHub> hub = std::make_shared<BroadcastHub>();
    return hub;
}

}  // namespace

StudentWebSocketController::StudentWebSocketController()
    : broadcast_hub_(configuredBroadcastHub())
{
}

void StudentWebSocketController::setBroadcastHub(std::shared_ptr<BroadcastHub> broadcastHub)
{
    configuredBroadcastHub() = std::move(broadcastHub);
}

void StudentWebSocketController::handleNewMessage(const drogon::WebSocketConnectionPtr& connection,
                                                  std::string&& message,
                                                  const drogon::WebSocketMessageType& type)
{
    if (type == drogon::WebSocketMessageType::Ping || message == "ping") {
        connection->send("pong");
    }
}

void StudentWebSocketController::handleNewConnection(
    const drogon::HttpRequestPtr& request,
    const drogon::WebSocketConnectionPtr& connection)
{
    const auto scope = request->getParameter(std::string(config::webSocketScopeParameter));
    if (scope == std::string(config::webSocketSchoolScope)) {
        broadcast_hub_->join(std::string(config::schoolWebSocketRoomId), connection);
        return;
    }

    const auto studentId = request->getParameter(std::string(config::webSocketStudentIdParameter));
    if (!domain::isSafeId(studentId)) {
        connection->shutdown(drogon::CloseCode::kViolation, "invalid student id");
        return;
    }
    broadcast_hub_->join(studentId, connection);
}

void StudentWebSocketController::handleConnectionClosed(
    const drogon::WebSocketConnectionPtr& connection)
{
    broadcast_hub_->leave(connection);
}

}  // namespace schoolmanager::adapters
