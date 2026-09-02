#include "rate_limiter/Config.h"

#include <stdexcept>
#include <string>

Config parseArguments(int argc, char* argv[]) {
    Config config;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--algorithm") {
            if (i + 1 >= argc) {
                throw std::runtime_error(
                    "--algorithm requires a value"
                );
            }

            config.algorithm = argv[++i];
        }

        else if (arg == "--capacity") {
            if (i + 1 >= argc) {
                throw std::runtime_error(
                    "--capacity requires a value"
                );
            }

            config.capacity = std::stoi(argv[++i]);
        }

        else if (arg == "--refill-rate") {
            if (i + 1 >= argc) {
                throw std::runtime_error(
                    "--refill-rate requires a value"
                );
            }

            config.refill_rate = std::stod(argv[++i]);
        }

        else if (arg == "--redis-url") {
            if (i + 1 >= argc) {
                throw std::runtime_error(
                    "--redis-url requires a value"
                );
            }

            config.redis_url = argv[++i];
        }

        else if (arg == "--client") {
            if (i + 1 >= argc) {
                throw std::runtime_error(
                    "--client requires a value"
                );
            }

            config.client_id = argv[++i];
        }

        else if (arg == "--limit") {
            config.limit = std::stoi(argv[++i]);
        }
        
        else if (arg == "--window") {
            config.window = std::stoi(argv[++i]);
        }

        else if (arg == "--leak-rate") {

            if (i + 1 >= argc) {
                throw std::runtime_error(
                "--leak-rate requires a value"
                );
            }
            config.leak_rate = std::stod(argv[++i]);
        }

        else {
            throw std::runtime_error(
                "Unknown argument: " + arg
            );
        }
    }

    return config;
}