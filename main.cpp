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
            
    } catch (const Error &err) {
        std::cerr << "Redis error: "
                  << err.what()
                  << "\n";

    } catch (const std::exception &err) {
        std::cerr << "Error: "
                  << err.what()
                  << "\n";
    }
    return 0;
}