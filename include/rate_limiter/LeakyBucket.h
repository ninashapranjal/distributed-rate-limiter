#pragma once

#include "rate_limiter/RateLimiter.h"

#include <sw/redis++/redis++.h>

#include <string>

class LeakyBucket : public RateLimiter {
public:
    LeakyBucket(
        sw::redis::Redis& redis,
        int capacity,
        double leak_rate,
        const std::string& script_path
    );

    bool allow(
        const std::string& client_id,
        int requested = 1
    ) override;

private:
    sw::redis::Redis& redis_;

    int capacity_;
    double leak_rate_;

    std::string script_sha_;
};