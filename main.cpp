#include "rate_limiter/TokenBucket.h"

#include <sw/redis++/redis++.h>

#include <iostream>
#include <thread>
#include <chrono>

int main() {
    try {
        sw::redis::Redis redis(
            "tcp://127.0.0.1:6379"
        );

        std::cout << "Connected to Redis\n";

        TokenBucket limiter(
            redis,
            10,                 // capacity
            1.0,                // refill rate
            "lua/token_bucket.lua"
        );

        for (int i = 1; i <= 15; i++) {

            bool allowed = limiter.allow(
                "user1"
            );

            if (allowed) {
                std::cout
                    << "Request "
                    << i
                    << ": ALLOWED\n";
            }
            else {
                std::cout
                    << "Request "
                    << i
                    << ": BLOCKED\n";
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds(100)
            );
        }

    }
    catch (const sw::redis::Error& err) {
        std::cerr
            << "Redis error: "
            << err.what()
            << "\n";
    }
    catch (const std::exception& err) {
        std::cerr
            << "Error: "
            << err.what()
            << "\n";
    }

    return 0;
}