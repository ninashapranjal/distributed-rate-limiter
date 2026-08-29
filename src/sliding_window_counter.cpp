#include "rate_limiter/SlidingWindowCounter.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace sw::redis;

namespace {

std::string loadScript(const std::string& filename) {
    std::ifstream file(filename);

    if (!file.is_open()) {
        throw std::runtime_error(
            "Could not open Lua script: " + filename
        );
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return buffer.str();
}

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

    // find which window we are currently in
    long long current_window =
        now / window_seconds_;

    // how far into the current window we are
    long long elapsed =
        now % window_seconds_;

    // redis key for current window
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

    std::string script =
        loadScript("lua/sliding_window_counter.lua");

    auto result = redis_.eval<long long>(
        script,
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