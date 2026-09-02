#include "rate_limiter/SlidingWindowLog.h"

#include <chrono>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace sw::redis;

namespace {

std::string loadScript(const std::string& filename) {

    std::ifstream file(filename);

    if (!file.is_open()) {
        throw std::runtime_error(
            "Could not open Lua script: " + filename
        );
    }

    return std::string{
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    };
}

double currentTime() {

    return std::chrono::duration<double>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

}

SlidingWindowLog::SlidingWindowLog(
    Redis& redis,
    int limit,
    int window_seconds,
    const std::string& script_path
)
    : redis_(redis),
      limit_(limit),
      window_seconds_(window_seconds)
{
    std::string script = loadScript(script_path);

    script_sha_ = redis_.script_load(script);
}

bool SlidingWindowLog::allow(
    const std::string& client_id,
    int requested
) {

    std::string key =
        "ratelimit:sliding-log:" + client_id;

    double now = currentTime();

    std::vector<std::string> keys = {
        key
    };

    std::vector<std::string> args = {
        std::to_string(now),
        std::to_string(window_seconds_),
        std::to_string(limit_),
        std::to_string(requested)
    };

    auto result = redis_.evalsha<long long>(
        script_sha_,
        keys.begin(),
        keys.end(),
        args.begin(),
        args.end()
    );

    return result == 1;
}