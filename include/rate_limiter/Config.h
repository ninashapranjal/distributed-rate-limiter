#pragma once

#include <string>

struct Config {
    std::string algorithm = "token-bucket";

    int capacity = 10;
    double refill_rate = 1.0;

    std::string redis_url = "tcp://127.0.0.1:6379";

    std::string client_id = "user1";
};

Config parseArguments(int argc, char* argv[]);