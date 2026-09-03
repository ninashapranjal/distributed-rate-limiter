#include "rate_limiter/SlidingWindowCounter.h"

#include <chrono>
#include <stdexcept>
#include <string>

using namespace sw::redis;

namespace {

const char* SLIDING_COUNTER_SCRIPT = R"(
    local base_key = KEYS[1]
    local window = tonumber(ARGV[1])
    local requested = tonumber(ARGV[2])
    local limit = tonumber(ARGV[3])
    local time = redis.call("TIME")
    local now = tonumber(time[1]) + tonumber(time[2]) / 1000000
    local current_window = math.floor(now / window)
    local elapsed = now - current_window * window
    local current_key = base_key .. ":" .. current_window
    local previous_key = base_key .. ":" .. (current_window - 1)
    local current_count = tonumber(redis.call("GET", current_key)) or 0
    local previous_count = tonumber(redis.call("GET", previous_key)) or 0
    local estimated_count = previous_count * ((window - elapsed) / window) + current_count

    if estimated_count + requested <= limit then
        redis.call("INCRBY", current_key, requested)
        -- This bucket is read throughout the following window as the previous
        -- bucket, so it must survive for two windows from its first write.
        if current_count == 0 then
            redis.call("EXPIRE", current_key, window * 2)
        end
        return 1
    end
    return 0
)";

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
    if (limit_ <= 0 || window_seconds_ <= 0) {
        throw std::invalid_argument("limit and window length must be positive");
    }
    script_sha_ = redis_.script_load(SLIDING_COUNTER_SCRIPT);
}

bool SlidingWindowCounter::allow(
    const std::string& client_id,
    int requested
) {
    if (requested <= 0) {
        throw std::invalid_argument("requested tokens must be positive");
    }

    const std::string key = "ratelimit:sliding-counter:" + client_id;

    auto result = redis_.evalsha<long long>(
        script_sha_,
        {key},
        {
            std::to_string(window_seconds_),
            std::to_string(requested),
            std::to_string(limit_)
        }
    );

    return result == 1;
}
