#pragma once

#include <drogon/WebSocketConnection.h>
#include <json/json.h>

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace schoolmanager::adapters {

struct WebSocketContext {
    std::string student_id;
};

class BroadcastHub {
  public:
    void join(const std::string& studentId, const drogon::WebSocketConnectionPtr& connection);
    void leave(const drogon::WebSocketConnectionPtr& connection);
    void broadcast(const std::string& studentId,
                   const Json::Value& message,
                   const drogon::WebSocketConnectionPtr& except = nullptr);

  private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::vector<std::weak_ptr<drogon::WebSocketConnection>>> rooms_;
};

}  // namespace schoolmanager::adapters
