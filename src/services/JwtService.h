#pragma once
#include <json/json.h>
#include <optional>
#include <string>

namespace col {

class JwtService {
public:
    static JwtService &instance();

    void setSecret(const std::string &secret);
    std::string generate(int64_t userId, const std::string &username);
    std::optional<Json::Value> verify(const std::string &token);

private:
    JwtService() = default;
    std::string secret_{"col-jwt-secret-change-in-prod"};
};

}
