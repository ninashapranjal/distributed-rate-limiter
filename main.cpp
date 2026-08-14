#include <sw/redis++/redis++.h>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

using namespace sw::redis;
using namespace std;

string loadScript(const string& filename) {
    ifstream file(filename);

    if (!file.is_open()) {
        throw runtime_error("Could not open Lua script");
    }

    return string(
        (istreambuf_iterator<char>(file)),
        istreambuf_iterator<char>()
    );
}

int main() {
    try {
        Redis redis("tcp://127.0.0.1:6379");
        cout << "Connected to Redis!\n";

        string script = loadScript("token_bucket.lua");

        auto script_sha = redis.script_load(script);

        cout << "Loaded Lua script:\n";
        cout << script_sha << "\n\n";

        string key = "ratelimit:user1";

        int capacity = 10;
        double refill_rate = 1;  
        int requested = 1;

        for (int i = 1; i <= 15; i++) {
            auto now = chrono::duration<double>(
                chrono::system_clock::now().time_since_epoch()
            ).count();

            vector<string> keys = {
                key
            };

            vector<string> args = {
                to_string(capacity),
                to_string(refill_rate),
                to_string(now),
                to_string(requested)
            };

            auto result = redis.evalsha<vector<long long>>(
                script_sha,
                keys.begin(),
                keys.end(),
                args.begin(),
                args.end()
            );

            int allowed = static_cast<int>(result[0]);
            double remaining = result[1];

            if (allowed) {
                cout
                    << "Request "
                    << i
                    << ": ALLOWED | Tokens left: "
                    << remaining
                    << "\n";
            }
            else {
                cout
                    << "Request "
                    << i
                    << ": BLOCKED | Tokens left: "
                    << remaining
                    << "\n";
            }

            this_thread::sleep_for(
                chrono::milliseconds(100)
            );
        }

    } catch (const Error &err) {
        cerr
            << "Redis error: "
            << err.what()
            << "\n";


    } catch (const exception &err) {
        cerr
            << "Error: "
            << err.what()
            << "\n";
    }
    
    return 0;
}

