#include "rate_limiter/SlidingWindowCounter.h"

#include <chrono>
#include <cmath>
#include <string>

using namespace sw::redis;

namespace {

const char* SLIDING_WINDOW_SCRIPT = R"(
    local current_count = tonumber(redis.call("GET", KEYS[1])) or 0
    local previous_count = tonumber(redis.call("GET", KEYS[2])) or 0

    local elapsed = tonumber(ARGV[1])
    local window = tonumber(ARGV[2])
    local requested = tonumber(ARGV[3])
    local limit = tonumber(ARGV[4])

    local previous_weight = (window - elapsed) / window

    local estimated_count =
        previous_count * previous_weight + current_count

    if estimated_count + requested <= limit then
        redis.call("INCRBY", KEYS[1], requested)

        redis.call(
            "EXPIRE",
            KEYS[1],
            window
        )

        return 1
    end

    return 0
)";

long long currentTimeSeconds() {
    return std::chrono::duration_cast<
        std::chrono::seconds
    >(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

}

SlidingWindowCounter::SlidingWindowCounter(
    Redis& redis,
    int limit,
    int window_seconds
)
    : redis_(redis),
      limit_(limit),
      window_seconds_(window_seconds)
{
}

bool SlidingWindowCounter::allow(
    const std::string& client_id,
    int requested
) {
    long long now = currentTimeSeconds();

    long long current_window =
        now / window_seconds_;

    long long elapsed =
        now % window_seconds_;

    std::string current_key =
        "ratelimit:sliding-counter:" +
        client_id +
        ":" +
        std::to_string(current_window);

    std::string previous_key =
        "ratelimit:sliding-counter:" +
        client_id +
        ":" +
        std::to_string(current_window - 1);

    auto result = redis_.eval<long long>(
        SLIDING_WINDOW_SCRIPT,
        {current_key, previous_key},
        {
            std::to_string(elapsed),
            std::to_string(window_seconds_),
            std::to_string(requested),
            std::to_string(limit_)
        }
    );

    return result == 1;
}