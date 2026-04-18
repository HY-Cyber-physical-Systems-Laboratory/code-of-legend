#pragma once
#include <drogon/HttpController.h>

namespace col {

class AuthController : public drogon::HttpController<AuthController>
{
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AuthController::signup, "/api/auth/register", drogon::Post);
    ADD_METHOD_TO(AuthController::login, "/api/auth/login", drogon::Post);
    ADD_METHOD_TO(AuthController::me, "/api/auth/me", drogon::Get, "col::AuthFilter");
    METHOD_LIST_END

    void signup(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    void login(const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    void me(const drogon::HttpRequestPtr &req,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};

}
