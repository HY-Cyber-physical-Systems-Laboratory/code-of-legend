#include "filters/AuthFilter.h"
#include "services/JwtService.h"

namespace col {

static drogon::HttpResponsePtr unauthorized(const std::string &msg)
{
    Json::Value body;
    body["error"] = msg;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(std::move(body));
    resp->setStatusCode(drogon::k401Unauthorized);
    return resp;
}

void AuthFilter::doFilter(const drogon::HttpRequestPtr &req,
                          drogon::FilterCallback &&cb,
                          drogon::FilterChainCallback &&ccb)
{
    auto auth = req->getHeader("Authorization");
    if (auth.size() <= 7 || auth.substr(0, 7) != "Bearer ") {
        cb(unauthorized("missing or invalid token"));
        return;
    }

    auto claims = JwtService::instance().verify(auth.substr(7));
    if (!claims) {
        cb(unauthorized("invalid or expired token"));
        return;
    }

    req->attributes()->insert("user_id", (*claims)["user_id"].asInt());
    req->attributes()->insert("username", (*claims)["username"].asString());
    ccb();
}

}
