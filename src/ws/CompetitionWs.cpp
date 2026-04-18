#include "ws/CompetitionWs.h"
#include "services/JudgeService.h"
#include "services/JwtService.h"
#include "services/MatchService.h"
#include <drogon/drogon.h>
#include <thread>

using namespace drogon;
using namespace drogon::orm;

namespace col {

static std::string toJson(const Json::Value &val)
{
    Json::StreamWriterBuilder b;
    b["indentation"] = "";
    return Json::writeString(b, val);
}

static std::string verdictStr(JudgeService::Verdict v)
{
    switch (v) {
    case JudgeService::Verdict::Accepted:          return "accepted";
    case JudgeService::Verdict::WrongAnswer:       return "wrong_answer";
    case JudgeService::Verdict::TimeLimitExceeded: return "tle";
    case JudgeService::Verdict::RuntimeError:      return "runtime_error";
    }
    return "unknown";
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

    // ── matchmaking ──
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

    // ── room management ──
    else if (action == "join_room") {
        auto roomId = json["room_id"].asString();
        if (roomId.empty()) return;

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
            room->players[ctx->username] = {100, 0, true, {}, conn};
        }
        ctx->roomId = roomId;

        Json::Value notify;
        notify["type"] = "player_joined";
        notify["username"] = ctx->username;
        broadcast(room, notify);
        broadcastHp(room);
    }
    else if (action == "leave_room") {
        if (ctx->roomId.empty()) return;
        std::shared_ptr<Room> room;
        {
            std::lock_guard lock(roomsMtx_);
            auto it = rooms_.find(ctx->roomId);
            if (it == rooms_.end()) return;
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

    // ── submit & judge ──
    else if (action == "submit") {
        if (ctx->roomId.empty()) return;

        auto problemId = json["problem_id"].asInt();
        auto language = json["language"].asString();
        auto code = json["code"].asString();

        std::shared_ptr<Room> room;
        {
            std::lock_guard lock(roomsMtx_);
            auto it = rooms_.find(ctx->roomId);
            if (it == rooms_.end()) return;
            room = it->second;
        }

        {
            std::lock_guard lock(room->mtx);
            if (room->gameOver) return;
            auto pit = room->players.find(ctx->username);
            if (pit == room->players.end() || !pit->second.alive) return;
            if (pit->second.solved.count(problemId)) {
                Json::Value err;
                err["type"] = "already_solved";
                err["problem_id"] = problemId;
                conn->send(toJson(err));
                return;
            }
        }

        Json::Value judging;
        judging["type"] = "judge_result";
        judging["username"] = ctx->username;
        judging["problem_id"] = problemId;
        judging["verdict"] = "judging";
        broadcast(room, judging);

        auto username = ctx->username;
        std::thread([this, room, username, problemId, language, code]() {
            auto res = JudgeService::instance().judge(problemId, language, code);

            app().getLoop()->queueInLoop([this, room, username, problemId, res]() {
                auto *prob = JudgeService::instance().getProblem(problemId);
                if (!prob) return;

                Json::Value verdict;
                verdict["type"] = "judge_result";
                verdict["username"] = username;
                verdict["problem_id"] = problemId;
                verdict["verdict"] = verdictStr(res.verdict);
                verdict["runtime_ms"] = res.runtimeMs;

                if (res.verdict == JudgeService::Verdict::Accepted) {
                    int damage = prob->damage;
                    verdict["damage"] = damage;

                    std::string killed;
                    {
                        std::lock_guard lock(room->mtx);
                        if (room->gameOver) return;

                        auto pit = room->players.find(username);
                        if (pit == room->players.end()) return;

                        pit->second.solved.insert(problemId);
                        pit->second.score += damage;

                        for (auto &[u, st] : room->players) {
                            if (u != username && st.alive) {
                                st.hp = std::max(0, st.hp - damage);
                                if (st.hp <= 0) {
                                    st.alive = false;
                                    killed = u;
                                }
                            }
                        }
                    }

                    broadcast(room, verdict);
                    broadcastHp(room);

                    if (!killed.empty()) {
                        std::lock_guard lock(room->mtx);
                        room->gameOver = true;

                        Json::Value go;
                        go["type"] = "game_over";
                        go["winner"] = username;
                        go["loser"] = killed;
                        go["reason"] = "kill";

                        auto str = toJson(go);
                        for (auto &[u, st] : room->players)
                            if (st.conn->connected())
                                st.conn->send(str);
                    }
                } else {
                    broadcast(room, verdict);
                }
            });
        }).detach();
    }

    // ── time up ──
    else if (action == "time_up") {
        if (ctx->roomId.empty()) return;

        std::shared_ptr<Room> room;
        {
            std::lock_guard lock(roomsMtx_);
            auto it = rooms_.find(ctx->roomId);
            if (it == rooms_.end()) return;
            room = it->second;
        }

        std::lock_guard lock(room->mtx);
        if (room->gameOver) return;
        room->gameOver = true;

        std::string winner, loser;
        int maxScore = -1;
        for (auto &[u, st] : room->players) {
            if (st.score > maxScore) {
                loser = winner;
                winner = u;
                maxScore = st.score;
            } else {
                loser = u;
            }
        }

        Json::Value go;
        go["type"] = "game_over";
        go["winner"] = winner;
        go["loser"] = loser;
        go["reason"] = "time";

        auto str = toJson(go);
        for (auto &[u, st] : room->players)
            if (st.conn->connected())
                st.conn->send(str);
    }
}

void CompetitionWs::handleConnectionClosed(const WebSocketConnectionPtr &conn)
{
    auto ctx = conn->getContext<WsContext>();
    if (!ctx) return;

    if (ctx->inQueue)
        MatchService::instance().dequeue(ctx->username);

    if (!ctx->roomId.empty()) {
        std::shared_ptr<Room> room;
        {
            std::lock_guard lock(roomsMtx_);
            auto it = rooms_.find(ctx->roomId);
            if (it == rooms_.end()) return;
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

void CompetitionWs::broadcast(const std::shared_ptr<Room> &room, const Json::Value &msg)
{
    auto str = toJson(msg);
    std::lock_guard lock(room->mtx);
    for (auto &[u, st] : room->players)
        if (st.conn->connected())
            st.conn->send(str);
}

void CompetitionWs::broadcastHp(const std::shared_ptr<Room> &room)
{
    Json::Value msg;
    msg["type"] = "hp_update";

    std::lock_guard lock(room->mtx);
    for (auto &[u, st] : room->players) {
        Json::Value p;
        p["hp"] = st.hp;
        p["score"] = st.score;
        p["alive"] = st.alive;
        msg["players"][u] = p;
    }

    auto str = toJson(msg);
    for (auto &[u, st] : room->players)
        if (st.conn->connected())
            st.conn->send(str);
}

}
