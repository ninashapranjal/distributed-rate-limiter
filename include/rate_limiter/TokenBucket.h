#pragma once

#include "RateLimiter.h"
#include <sw/redis++/redis++.h>

//inherits from RateLimiter
class TokenBucket : public RateLimiter {
public:
    TokenBucket(
        sw::redis::Redis& redis,
        int capacity, 
        double refill_rate,
        const std::string& script_path //file path to lua script
    );

    bool allow(
        const std::string& client_id,
        int requested = 1 //default costs one token
    ) override;

private:
    sw::redis::Redis& redis_;

    int capacity_;
    double refill_rate_;

	//unique string hash given by redis when lua script loaded
    std::string script_sha_; 
};