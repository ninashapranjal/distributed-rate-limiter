#include "rate_limiter/TokenBucket.h"
#include "rate_limiter/Config.h"

#include <sw/redis++/redis++.h>

#include <iostream>
#include <thread>
#include <chrono>

int main(int argc, char* argv[]) {

    Config config = parseArguments(argc, argv);

    try {
        sw::redis::Redis redis(config.redis_url);

        std::cout << "Connected to Redis\n";

        TokenBucket limiter(
            redis,
            config.capacity,
            config.refill_rate,
            "lua/token_bucket.lua"
            );

        for (int i = 1; i <= 15; i++) {

            bool allowed = limiter.allow(config.client_id);

            if (allowed) {
                std::cout << "Request " << i << ": ALLOWED\n";
            }
            else {
                std::cout << "Request " << i << ": BLOCKED\n";
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