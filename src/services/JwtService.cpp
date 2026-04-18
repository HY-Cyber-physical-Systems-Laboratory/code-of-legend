#include "services/JwtService.h"
#include <jwt-cpp/jwt.h>

namespace col {

JwtService &JwtService::instance()
{
    static JwtService inst;
    return inst;
}

void JwtService::setSecret(const std::string &secret)
{
    secret_ = secret;
}

std::string JwtService::generate(int64_t userId, const std::string &username)
{
    return jwt::create()
        .set_issuer("code-of-legend")
        .set_type("JWT")
        .set_payload_claim("user_id", jwt::claim(std::to_string(userId)))
        .set_payload_claim("username", jwt::claim(username))
        .set_issued_at(std::chrono::system_clock::now())
        .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours{24})
        .sign(jwt::algorithm::hs256{secret_});
}

std::optional<Json::Value> JwtService::verify(const std::string &token)
{
    try {
        auto decoded = jwt::decode(token);
        jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{secret_})
            .with_issuer("code-of-legend")
            .verify(decoded);

        Json::Value claims;
        claims["user_id"] = static_cast<Json::Int64>(
            std::stoll(decoded.get_payload_claim("user_id").as_string()));
        claims["username"] = decoded.get_payload_claim("username").as_string();
        return claims;
    } catch (...) {
        return std::nullopt;
    }
}

}
