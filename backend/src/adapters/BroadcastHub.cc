#include "schoolmanager/adapters/BroadcastHub.h"

#include "schoolmanager/adapters/JsonHelpers.h"

#include <algorithm>

namespace schoolmanager::adapters {

void BroadcastHub::join(const std::string& studentId,
                        const drogon::WebSocketConnectionPtr& connection)
{
    connection->setContext(std::make_shared<WebSocketContext>(WebSocketContext{studentId}));
    std::lock_guard<std::mutex> guard(mutex_);
    rooms_[studentId].push_back(connection);
}

void BroadcastHub::leave(const drogon::WebSocketConnectionPtr& connection)
{
    const auto context = connection->getContext<WebSocketContext>();
    if (!context) {
        return;
    }

    std::lock_guard<std::mutex> guard(mutex_);
    auto it = rooms_.find(context->student_id);
    if (it == rooms_.end()) {
        return;
    }

    auto& connections = it->second;
    connections.erase(
        std::remove_if(connections.begin(),
                       connections.end(),
                       [&](const std::weak_ptr<drogon::WebSocketConnection>& weak) {
                           auto locked = weak.lock();
                           return !locked || locked == connection;
                       }),
        connections.end());
    if (connections.empty()) {
        rooms_.erase(it);
    }
}

void BroadcastHub::broadcast(const std::string& studentId,
                             const Json::Value& message,
                             const drogon::WebSocketConnectionPtr& except)
{
    std::vector<drogon::WebSocketConnectionPtr> targets;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        auto it = rooms_.find(studentId);
        if (it == rooms_.end()) {
            return;
        }

        auto& connections = it->second;
        connections.erase(
            std::remove_if(connections.begin(),
                           connections.end(),
                           [&](const std::weak_ptr<drogon::WebSocketConnection>& weak) {
                               auto locked = weak.lock();
                               if (!locked || locked->disconnected()) {
                                   return true;
                               }
                               if (except && locked == except) {
                                   return false;
                               }
                               targets.push_back(locked);
                               return false;
                           }),
            connections.end());
    }

    const auto payload = writeJsonCompact(message);
    for (const auto& target : targets) {
        if (!except || target != except) {
            target->send(payload);
        }
    }
}

}  // namespace schoolmanager::adapters
