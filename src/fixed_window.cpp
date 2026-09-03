#include "rate_limiter/FixedWindow.h"

#include <chrono>
#include <stdexcept>
#include <string>

using namespace sw::redis;

namespace {

const char* FIXED_WINDOW_SCRIPT = R"(
    local requested = tonumber(ARGV[1])
    local window = tonumber(ARGV[2])
    local limit = tonumber(ARGV[3])
    local time = redis.call("TIME")
    local now = tonumber(time[1])
    local window_id = math.floor(now / window)
    local key = KEYS[1] .. ":" .. window_id
    local current = tonumber(redis.call("GET", key)) or 0

    if current + requested <= limit then
        redis.call("INCRBY", key, requested)
        -- Expire at the next global window boundary, not one full window after
        -- the first request. This preserves fixed-window alignment.
        if current == 0 then
            local ttl = window - (now % window)
            redis.call("EXPIRE", key, ttl)
        end
        return 1
    end

    return 0
)";

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
    if (limit_ <= 0 || window_seconds_ <= 0) {
        throw std::invalid_argument("limit and window length must be positive");
    }
}

bool FixedWindow::allow(
    const std::string& client_id,
    int requested
) {
    if (requested <= 0) {
        throw std::invalid_argument("requested tokens must be positive");
    }

    const std::string key = "ratelimit:fixed:" + client_id;

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
