#include "rate_limiter/TokenBucket.h"

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <chrono> //measure precise time
#include <vector>

using namespace sw::redis;
using namespace std;

namespace { //helper func below can only be used in this file

string loadScript(const string& filename) {
    ifstream file(filename);

    if (!file.is_open()) {
        throw runtime_error("Could not open Lua script: " + filename);
    }

    return string{istreambuf_iterator<char>(file), istreambuf_iterator<char>()};
}

double currentTime() {
    return chrono::duration<double>(
        chrono::system_clock::now().time_since_epoch()
    ).count();
}

}

TokenBucket::TokenBucket(
    Redis& redis,
    int capacity,
    double refill_rate,
    const string& script_path
)
    : redis_(redis),
      capacity_(capacity),
      refill_rate_(refill_rate)
{
    string script = loadScript(script_path);

    script_sha_ = redis_.script_load(script);
}

bool TokenBucket::allow(
    const string& client_id,
    int requested
) {
    string key = "ratelimit:" + client_id;

    double now = currentTime();

    vector<string> keys = {
        key
    };

    vector<string> args = {
        to_string(capacity_),
        to_string(refill_rate_),
        to_string(now),
        to_string(requested)
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