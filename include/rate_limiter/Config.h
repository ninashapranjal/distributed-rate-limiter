#pragma once

#include <string>

struct Config {
    std::string algorithm = "token-bucket";

    // for token bucket
    int capacity = 10;
    double refill_rate = 1.0;

    // for leaky Bucket
    double leak_rate = 1.0;

    // for fixed window and sliding window
    int limit = 100;
    int window = 60;

    std::string redis_url = "tcp://127.0.0.1:6379";
    std::string client_id = "user1";
};

Config parseArguments(int argc, char* argv[]);