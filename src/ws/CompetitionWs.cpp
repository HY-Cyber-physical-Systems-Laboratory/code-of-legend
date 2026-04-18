#include "ws/CompetitionWs.h"
#include "services/JwtService.h"
#include "services/MatchService.h"
#include <drogon/drogon.h>

using namespace drogon;
using namespace drogon::orm;

namespace col {

static std::string toJson(const Json::Value &val)
{
    Json::StreamWriterBuilder b;
    b["indentation"] = "";
    return Json::writeString(b, val);
}

void CompetitionWs::handleNewConnection(const HttpRequestPtr &req,
                                        const WebSocketConnectionPtr &conn)
{
    auto token = req->getParameter("token");
    auto claims = JwtService::instance().verify(token);
    if (!claims) {
        Json::Value err;
        err["type"] = "error";
        err["message"] = "unauthorized";
        conn->send(toJson(err));
        conn->shutdown(CloseCode::kViolation, "unauthorized");
        return;
    }

    auto ctx = std::make_shared<WsContext>();
    ctx->username = (*claims)["username"].asString();
    ctx->userId = (*claims)["user_id"].asInt();
    conn->setContext(ctx);

    Json::Value msg;
    msg["type"] = "connected";
    msg["username"] = ctx->username;
    conn->send(toJson(msg));
}

void CompetitionWs::handleNewMessage(const WebSocketConnectionPtr &conn,
                                     std::string &&message,
                                     const WebSocketMessageType &type)
{
    if (type != WebSocketMessageType::Text)
        return;

    Json::Value json;
    Json::CharReaderBuilder rb;
    std::string errs;
    std::istringstream ss(message);
    if (!Json::parseFromStream(rb, ss, &json, &errs))
        return;

    auto ctx = conn->getContext<WsContext>();
    if (!ctx)
        return;

    auto action = json["action"].asString();

    if (action == "queue") {
        ctx->inQueue = true;
        auto db = app().getDbClient();
        db->execSqlAsync(
            "SELECT rating FROM users WHERE id = $1",
            [ctx, conn](const Result &r) {
                int rating = r.empty() ? 1000 : r[0]["rating"].as<int>();
                ctx->rating = rating;
                MatchService::instance().enqueue(ctx->username, rating, conn);
            },
            [](const DrogonDbException &) {},
            ctx->userId);
    }
    else if (action == "dequeue") {
        ctx->inQueue = false;
        MatchService::instance().dequeue(ctx->username);
    }
    else if (action == "join_room") {
        auto roomId = json["room_id"].asString();
        if (roomId.empty())
            return;

        if (!ctx->roomId.empty()) {
            std::lock_guard lock(roomsMtx_);
            auto it = rooms_.find(ctx->roomId);
            if (it != rooms_.end()) {
                std::lock_guard rl(it->second->mtx);
                it->second->players.erase(ctx->username);
            }
        }

        std::shared_ptr<Room> room;
        {
            std::lock_guard lock(roomsMtx_);
            auto &r = rooms_[roomId];
            if (!r) {
                r = std::make_shared<Room>();
                r->id = roomId;
            }
            room = r;
        }
        {
            std::lock_guard lock(room->mtx);
            room->players[ctx->username] = conn;
        }
        ctx->roomId = roomId;

        Json::Value notify;
        notify["type"] = "player_joined";
        notify["username"] = ctx->username;
        {
            std::lock_guard lock(room->mtx);
            notify["player_count"] = static_cast<int>(room->players.size());
        }
        broadcast(room, notify);
    }
    else if (action == "leave_room") {
        if (ctx->roomId.empty())
            return;

        std::shared_ptr<Room> room;
        {
            std::lock_guard lock(roomsMtx_);
            auto it = rooms_.find(ctx->roomId);
            if (it == rooms_.end())
                return;
            room = it->second;
        }
        {
            std::lock_guard lock(room->mtx);
            room->players.erase(ctx->username);
        }

        Json::Value notify;
        notify["type"] = "player_left";
        notify["username"] = ctx->username;
        broadcast(room, notify);
        ctx->roomId.clear();
    }
    else if (action == "submit") {
        if (ctx->roomId.empty())
            return;

        std::shared_ptr<Room> room;
        {
            std::lock_guard lock(roomsMtx_);
            auto it = rooms_.find(ctx->roomId);
            if (it == rooms_.end())
                return;
            room = it->second;
        }

        Json::Value notify;
        notify["type"] = "submission";
        notify["username"] = ctx->username;
        notify["problem_id"] = json["problem_id"];
        notify["status"] = "judging";
        broadcast(room, notify);
    }
}

void CompetitionWs::handleConnectionClosed(const WebSocketConnectionPtr &conn)
{
    auto ctx = conn->getContext<WsContext>();
    if (!ctx)
        return;

    if (ctx->inQueue)
        MatchService::instance().dequeue(ctx->username);

    if (!ctx->roomId.empty()) {
        std::shared_ptr<Room> room;
        {
            std::lock_guard lock(roomsMtx_);
            auto it = rooms_.find(ctx->roomId);
            if (it == rooms_.end())
                return;
            room = it->second;
        }
        {
            std::lock_guard lock(room->mtx);
            room->players.erase(ctx->username);
        }

        Json::Value notify;
        notify["type"] = "player_left";
        notify["username"] = ctx->username;
        broadcast(room, notify);

        std::lock_guard lock(roomsMtx_);
        std::lock_guard rl(room->mtx);
        if (room->players.empty())
            rooms_.erase(ctx->roomId);
    }
}

void CompetitionWs::broadcast(const std::shared_ptr<Room> &room,
                              const Json::Value &msg,
                              const std::string &excludeUser)
{
    auto str = toJson(msg);
    std::lock_guard lock(room->mtx);
    for (auto &[user, c] : room->players)
        if (user != excludeUser)
            c->send(str);
}

}
