#pragma once
#include <drogon/WebSocketConnection.h>
#include <atomic>
#include <deque>
#include <mutex>
#include <string>

namespace col {

class MatchService {
public:
    static MatchService &instance();

    void enqueue(const std::string &username, int rating,
                 const drogon::WebSocketConnectionPtr &conn);
    void dequeue(const std::string &username);

private:
    MatchService() = default;
    void tryMatch();

    struct Entry {
        std::string username;
        int rating;
        drogon::WebSocketConnectionPtr conn;
    };

    std::deque<Entry> queue_;
    std::mutex mtx_;
    std::atomic<uint64_t> roomSeq_{0};

    static constexpr int MATCH_SIZE = 2;
};

}
