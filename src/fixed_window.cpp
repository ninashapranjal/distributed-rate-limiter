#include "rate_limiter/FixedWindow.h"

#include <chrono>
#include <stdexcept>
#include <string>

using namespace sw::redis;

namespace {

const char* FIXED_WINDOW_SCRIPT = R"(
    local current = redis.call("GET", KEYS[1])

    if not current then
        redis.call("SET", KEYS[1], ARGV[1], "EX", ARGV[2])
        return 1
    end

    if tonumber(current) + tonumber(ARGV[1]) <= tonumber(ARGV[3]) then
        redis.call("INCRBY", KEYS[1], ARGV[1])
        return 1
    end

    return 0
)";

long long currentWindow(
    int window_seconds
) {
    auto now = std::chrono::system_clock::now();

    auto seconds = std::chrono::duration_cast<
        std::chrono::seconds
    >(
        now.time_since_epoch()
    ).count();

    return seconds / window_seconds;
}

}

FixedWindow::FixedWindow(
    Redis& redis,
    int limit,
    int window_seconds
)
    : redis_(redis),
      limit_(limit),
      window_seconds_(window_seconds)
{
}

bool FixedWindow::allow(
    const std::string& client_id,
    int requested
) {
    long long window = currentWindow(window_seconds_);

    std::string key =
        "ratelimit:fixed:" +
        client_id +
        ":" +
        std::to_string(window);

    auto result = redis_.eval<long long>(
        FIXED_WINDOW_SCRIPT,
        {key},
        {
            std::to_string(requested),
            std::to_string(window_seconds_),
            std::to_string(limit_)
        }
    );

    return result == 1;
}