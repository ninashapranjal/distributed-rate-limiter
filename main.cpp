#include <sw/redis++/redis++.h>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

using namespace sw::redis;

std::string loadScript(const std::string& filename) {
    std::ifstream file(filename);

    if (!file.is_open()) {
        throw std::runtime_error("Could not open Lua script");
    }

    return std::string(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );
}

int main() {
    try {
        Redis redis("tcp://127.0.0.1:6379");
        std::cout << "Connected to Redis!\n";

        std::string script = loadScript("token_bucket.lua");

        auto script_sha = redis.script_load(script);

        std::cout << "Loaded Lua script:\n";
        std::cout << script_sha << "\n\n";

        std::string key = "ratelimit:user1";

        int capacity = 10;
        double refill_rate = 1;  
        int requested = 1;

        for (int i = 1; i <= 15; i++) {
            auto now = std::chrono::duration<double>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();

            std::vector<std::string> keys = {
                key
            };

            std::vector<std::string> args = {
                std::to_string(capacity),
                std::to_string(refill_rate),
                std::to_string(now),
                std::to_string(requested)
            };

            auto result = redis.evalsha<std::vector<long long>>(
                script_sha,
                keys.begin(),
                keys.end(),
                args.begin(),
                args.end()
            );

            int allowed = static_cast<int>(result[0]);
            double remaining = result[1];

            if (allowed) {
                std::cout
                    << "Request "
                    << i
                    << ": ALLOWED | Tokens left: "
                    << remaining
                    << "\n";
            }
            else {
                std::cout
                    << "Request "
                    << i
                    << ": BLOCKED | Tokens left: "
                    << remaining
                    << "\n";
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds(100)
            );
        }

    } catch (const Error &err) {
        std::cerr
            << "Redis error: "
            << err.what()
            << "\n";


    } catch (const std::exception &err) {
        std::cerr
            << "Error: "
            << err.what()
            << "\n";
    }
    
    return 0;
}

