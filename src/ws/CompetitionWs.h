#pragma once
#include <drogon/WebSocketController.h>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace col {

struct WsContext {
    std::string username;
    int userId = 0;
    int rating = 1000;
    std::string roomId;
    bool inQueue = false;
};

struct PlayerState {
    int hp = 100;
    int score = 0;
    bool alive = true;
    std::unordered_set<int> solved;
    drogon::WebSocketConnectionPtr conn;
};

struct Room {
    std::string id;
    std::unordered_map<std::string, PlayerState> players;
    bool gameOver = false;
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
    void broadcast(const std::shared_ptr<Room> &room, const Json::Value &msg);
    void broadcastHp(const std::shared_ptr<Room> &room);

    std::unordered_map<std::string, std::shared_ptr<Room>> rooms_;
    std::mutex roomsMtx_;
};

}
