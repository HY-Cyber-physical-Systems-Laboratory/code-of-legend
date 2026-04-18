#include "services/MatchService.h"
#include <algorithm>
#include <json/json.h>
#include <vector>

namespace col {

static std::string toJson(const Json::Value &val)
{
    Json::StreamWriterBuilder b;
    b["indentation"] = "";
    return Json::writeString(b, val);
}

MatchService &MatchService::instance()
{
    static MatchService inst;
    return inst;
}

void MatchService::enqueue(const std::string &username, int rating,
                           const drogon::WebSocketConnectionPtr &conn)
{
    std::lock_guard lock(mtx_);

    for (auto &e : queue_)
        if (e.username == username)
            return;

    queue_.push_back({username, rating, conn});

    Json::Value status;
    status["type"] = "queue_status";
    status["position"] = static_cast<int>(queue_.size());
    conn->send(toJson(status));

    tryMatch();
}

void MatchService::dequeue(const std::string &username)
{
    std::lock_guard lock(mtx_);
    std::erase_if(queue_, [&](const Entry &e) { return e.username == username; });
}

void MatchService::tryMatch()
{
    while (queue_.size() >= static_cast<size_t>(MATCH_SIZE)) {
        auto roomId = "room-" + std::to_string(++roomSeq_);

        std::vector<Entry> matched;
        for (int i = 0; i < MATCH_SIZE; i++) {
            matched.push_back(std::move(queue_.front()));
            queue_.pop_front();
        }

        Json::Value msg;
        msg["type"] = "matched";
        msg["room_id"] = roomId;

        Json::Value players(Json::arrayValue);
        for (auto &e : matched) {
            Json::Value p;
            p["username"] = e.username;
            p["rating"] = e.rating;
            players.append(p);
        }
        msg["players"] = players;

        auto str = toJson(msg);
        for (auto &e : matched)
            if (e.conn->connected())
                e.conn->send(str);
    }
}

}
