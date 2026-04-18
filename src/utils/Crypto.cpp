#include "utils/Crypto.h"
#include <argon2.h>
#include <openssl/rand.h>
#include <stdexcept>
#include <vector>

namespace col::crypto {

static constexpr uint32_t T_COST = 3;
static constexpr uint32_t M_COST = 1 << 16;
static constexpr uint32_t PARALLELISM = 4;
static constexpr size_t SALT_LEN = 16;
static constexpr size_t HASH_LEN = 32;
static constexpr size_t ENCODED_LEN = 256;

std::string hashPassword(const std::string &password)
{
    std::vector<uint8_t> salt(SALT_LEN);
    if (RAND_bytes(salt.data(), SALT_LEN) != 1)
        throw std::runtime_error("failed to generate salt");

    std::string encoded(ENCODED_LEN, '\0');
    int rc = argon2id_hash_encoded(
        T_COST, M_COST, PARALLELISM,
        password.data(), password.size(),
        salt.data(), salt.size(),
        HASH_LEN,
        encoded.data(), encoded.size());

    if (rc != ARGON2_OK)
        throw std::runtime_error(argon2_error_message(rc));

    encoded.resize(encoded.find('\0'));
    return encoded;
}

bool verifyPassword(const std::string &password, const std::string &hash)
{
    return argon2id_verify(hash.c_str(), password.data(), password.size()) == ARGON2_OK;
}

}
