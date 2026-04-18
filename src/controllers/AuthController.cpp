#include "controllers/AuthController.h"
#include "services/JwtService.h"
#include "utils/Crypto.h"

using namespace drogon;
using namespace drogon::orm;

namespace col {

static HttpResponsePtr jsonError(const std::string &msg, HttpStatusCode code)
{
    Json::Value body;
    body["error"] = msg;
    auto resp = HttpResponse::newHttpJsonResponse(std::move(body));
    resp->setStatusCode(code);
    return resp;
}

void AuthController::signup(const HttpRequestPtr &req,
                            std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto json = req->getJsonObject();
    if (!json) {
        callback(jsonError("invalid JSON body", k400BadRequest));
        return;
    }

    auto username = (*json)["username"].asString();
    auto email = (*json)["email"].asString();
    auto password = (*json)["password"].asString();

    if (username.empty() || email.empty() || password.empty()) {
        callback(jsonError("username, email, password required", k400BadRequest));
        return;
    }
    if (username.size() < 3 || username.size() > 20) {
        callback(jsonError("username must be 3-20 characters", k400BadRequest));
        return;
    }
    if (password.size() < 8) {
        callback(jsonError("password must be at least 8 characters", k400BadRequest));
        return;
    }

    std::string hash;
    try {
        hash = crypto::hashPassword(password);
    } catch (const std::exception &e) {
        callback(jsonError("internal error", k500InternalServerError));
        return;
    }

    auto db = app().getDbClient();
    db->execSqlAsync(
        "INSERT INTO users (username, email, password_hash) VALUES ($1, $2, $3) RETURNING id",
        [callback, username](const Result &r) {
            auto userId = r[0]["id"].as<int64_t>();
            auto token = JwtService::instance().generate(userId, username);

            Json::Value body;
            body["token"] = token;
            body["user"]["id"] = static_cast<Json::Int64>(userId);
            body["user"]["username"] = username;
            callback(HttpResponse::newHttpJsonResponse(std::move(body)));
        },
        [callback](const DrogonDbException &e) {
            std::string msg = e.base().what();
            if (msg.find("unique") != std::string::npos ||
                msg.find("duplicate") != std::string::npos)
                callback(jsonError("username or email already exists", k409Conflict));
            else
                callback(jsonError("registration failed", k500InternalServerError));
        },
        username, email, hash);
}

void AuthController::login(const HttpRequestPtr &req,
                           std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto json = req->getJsonObject();
    if (!json) {
        callback(jsonError("invalid JSON body", k400BadRequest));
        return;
    }

    auto username = (*json)["username"].asString();
    auto password = (*json)["password"].asString();

    if (username.empty() || password.empty()) {
        callback(jsonError("username and password required", k400BadRequest));
        return;
    }

    auto db = app().getDbClient();
    db->execSqlAsync(
        "SELECT id, username, password_hash FROM users WHERE username = $1",
        [callback, password](const Result &r) {
            if (r.empty() || !crypto::verifyPassword(password, r[0]["password_hash"].as<std::string>())) {
                callback(jsonError("invalid credentials", k401Unauthorized));
                return;
            }

            auto userId = r[0]["id"].as<int64_t>();
            auto uname = r[0]["username"].as<std::string>();
            auto token = JwtService::instance().generate(userId, uname);

            Json::Value body;
            body["token"] = token;
            body["user"]["id"] = static_cast<Json::Int64>(userId);
            body["user"]["username"] = uname;
            callback(HttpResponse::newHttpJsonResponse(std::move(body)));
        },
        [callback](const DrogonDbException &) {
            callback(jsonError("login failed", k500InternalServerError));
        },
        username);
}

void AuthController::me(const HttpRequestPtr &req,
                        std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto userId = req->attributes()->get<int>("user_id");
    auto db = app().getDbClient();

    db->execSqlAsync(
        "SELECT id, username, email, rating, created_at FROM users WHERE id = $1",
        [callback](const Result &r) {
            if (r.empty()) {
                callback(jsonError("user not found", k404NotFound));
                return;
            }
            Json::Value user;
            user["id"] = static_cast<Json::Int64>(r[0]["id"].as<int64_t>());
            user["username"] = r[0]["username"].as<std::string>();
            user["email"] = r[0]["email"].as<std::string>();
            user["rating"] = r[0]["rating"].as<int>();
            user["created_at"] = r[0]["created_at"].as<std::string>();
            callback(HttpResponse::newHttpJsonResponse(std::move(user)));
        },
        [callback](const DrogonDbException &) {
            callback(jsonError("internal error", k500InternalServerError));
        },
        userId);
}

}
