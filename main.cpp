#include <sw/redis++/redis++.h>
#include <iostream>

int main() {
    try {
        sw::redis::Redis redis("tcp://127.0.0.1:6379");
        redis.set("foo", "bar");
        auto val = redis.get("foo");
        if (val) {
            std::cout << "Connected! foo = " << *val << std::endl;
        }
    } catch (const sw::redis::Error &e) {
        std::cerr << "Redis error: " << e.what() << std::endl;
    }
    return 0;
}