#pragma once

#include "schoolmanager/adapters/BroadcastHub.h"

#include <drogon/WebSocketController.h>

#include <memory>

namespace schoolmanager::adapters {

class StudentWebSocketController : public drogon::WebSocketController<StudentWebSocketController> {
  public:
    StudentWebSocketController();

    static void setBroadcastHub(std::shared_ptr<BroadcastHub> broadcastHub);

    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/api/ws/students", drogon::Get);
    WS_PATH_LIST_END

    void handleNewMessage(const drogon::WebSocketConnectionPtr& connection,
                          std::string&& message,
                          const drogon::WebSocketMessageType& type) override;
    void handleNewConnection(const drogon::HttpRequestPtr& request,
                             const drogon::WebSocketConnectionPtr& connection) override;
    void handleConnectionClosed(const drogon::WebSocketConnectionPtr& connection) override;

  private:
    std::shared_ptr<BroadcastHub> broadcast_hub_;
};

}  // namespace schoolmanager::adapters
