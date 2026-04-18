#pragma once
#include <string>

namespace col::crypto {

std::string hashPassword(const std::string &password);
bool verifyPassword(const std::string &password, const std::string &hash);

}
