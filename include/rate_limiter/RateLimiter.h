#pragma once

#include <string>

class RateLimiter {
  public:
    virtual ~RateLimiter() = default;

    //pure virt func
    virtual bool allow(const std::string& client_id, int requested = 1) = 0;
};