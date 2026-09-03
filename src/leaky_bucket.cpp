#include "rate_limiter/LeakyBucket.h"

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

LeakyBucket::LeakyBucket(
    Redis& redis,
    int capacity,
    double leak_rate,
    const std::string& script_path
)
    : redis_(redis),
      capacity_(capacity),
      leak_rate_(leak_rate)
{
    if (capacity_ <= 0 || leak_rate_ < 0) {
        throw std::invalid_argument("capacity must be positive and leak rate cannot be negative");
    }

    std::string script = loadScript(script_path);

    script_sha_ = redis_.script_load(script);
}

bool LeakyBucket::allow(
    const std::string& client_id,
    int requested
) {
    if (requested <= 0) {
        throw std::invalid_argument("requested tokens must be positive");
    }

    std::string key =
        "ratelimit:leaky:" + client_id;

    std::vector<std::string> keys = {
        key
    };

    std::vector<std::string> args = {
        std::to_string(capacity_),
        std::to_string(leak_rate_),
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
