#pragma once
#include <drogon/WebSocketController.h>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace col {

struct WsContext {
    std::string username;
    int userId = 0;
    int rating = 1000;
    std::string roomId;
    bool inQueue = false;
};

struct Room {
    std::string id;
    std::unordered_map<std::string, drogon::WebSocketConnectionPtr> players;
    std::mutex mtx;
};

class CompetitionWs : public drogon::WebSocketController<CompetitionWs>
{
public:
    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/ws/competition", drogon::Get);
    WS_PATH_LIST_END

    void handleNewMessage(const drogon::WebSocketConnectionPtr &,
                          std::string &&,
                          const drogon::WebSocketMessageType &) override;
    void handleNewConnection(const drogon::HttpRequestPtr &,
                             const drogon::WebSocketConnectionPtr &) override;
    void handleConnectionClosed(const drogon::WebSocketConnectionPtr &) override;

private:
    void broadcast(const std::shared_ptr<Room> &room,
                   const Json::Value &msg,
                   const std::string &excludeUser = "");

    std::unordered_map<std::string, std::shared_ptr<Room>> rooms_;
    std::mutex roomsMtx_;
};

}
