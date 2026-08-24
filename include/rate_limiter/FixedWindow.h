#pragma once

#include "rate_limiter/RateLimiter.h"

#include <sw/redis++/redis++.h>

#include <string>

class FixedWindow : public RateLimiter {
public:
    FixedWindow(
        sw::redis::Redis& redis,
        int limit,
        int window_seconds
    );

    bool allow(
        const std::string& client_id,
        int requested = 1
    ) override;

private:
    sw::redis::Redis& redis_;

    int limit_;
    int window_seconds_;
};