#pragma once

#include "rate_limiter/RateLimiter.h"

#include <sw/redis++/redis++.h>

#include <string>

class SlidingWindowLog : public RateLimiter {
public:
    SlidingWindowLog(
        sw::redis::Redis& redis,
        int limit,
        int window_seconds,
        const std::string& script_path
    );

    bool allow(
        const std::string& client_id,
        int requested = 1
    ) override;

private:
    sw::redis::Redis& redis_;

    int limit_;
    int window_seconds_;

    std::string script_sha_;
};