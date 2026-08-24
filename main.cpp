#include "rate_limiter/Config.h"
#include "rate_limiter/FixedWindow.h"
#include "rate_limiter/TokenBucket.h"

#include <sw/redis++/redis++.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

int main(int argc, char* argv[]) {

    try {
        Config config = parseArguments(argc, argv);

        sw::redis::Redis redis(config.redis_url);

        std::cout << "Connected to Redis\n";

        std::unique_ptr<RateLimiter> limiter;

        if (config.algorithm == "token-bucket") {

            limiter = std::make_unique<TokenBucket>(
                redis,
                config.capacity,
                config.refill_rate,
                "lua/token_bucket.lua"
            );

        }
        else if (config.algorithm == "fixed-window") {

            limiter = std::make_unique<FixedWindow>(
                redis,
                config.limit,
                config.window
            );

        }
        else {
            throw std::runtime_error(
                "Unknown algorithm: " + config.algorithm
            );
        }

        for (int i = 1; i <= 15; i++) {

            bool allowed = limiter->allow(
                config.client_id
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

        return 1;
    }
    catch (const std::exception& err) {

        std::cerr
            << "Error: "
            << err.what()
            << "\n";

        return 1;
    }

    return 0;
}