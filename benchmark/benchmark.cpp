#include "rate_limiter/FixedWindow.h"
#include "rate_limiter/LeakyBucket.h"
#include "rate_limiter/RateLimiter.h"
#include "rate_limiter/SlidingWindowCounter.h"
#include "rate_limiter/SlidingWindowLog.h"
#include "rate_limiter/TokenBucket.h"

#include <sw/redis++/redis++.h>

#include <algorithm>
#include <string>
#include <thread>


namespace {


//benchmarking configuration
struct Options{
  std::string algorithm = "token-bucket"; //default
  std::size_t clients = 100;
  std::size_t clients = 100;
  std::size_t requests = 100000;
  std::size_t threads = std::max(1u, std::thread::hardware_concurrency());
  std::size_t pool_size = 0;
  int pool_wait_ms = 1000;
  int capacity = 10;
  double refill_rate = 1.0;
  int limit = 100;
  int window = 60;
  double leak_rate = 1.0;
  std::string redis_host = "127.0.0.1";
  int redis_port = 6379;
  std::string key_prefix = "benchmark";
};

//ensure all threads start benchmark at approx the same time
class StartSignal {
  public:
    explicit StartSignal(std::size_t participants) : participants_(participants) {}

    void arrive_and_wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (++arrived_ == participants_) {
            open_ = true;
            condition_.notify_all();
        } else {
            condition_.wait(lock, [this] { return open_; });
        }
    }

  private:
    const std::size_t participants_;
    std::size_t arrived_ = 0;
    bool open_ = false;
    std::mutex mutex_;
    std::condition_variable condition_;
};

void usage(const char* program) {
    std::cout
        << "Usage: " << program << " [options]\n\n"
        << "  --algorithm NAME       token-bucket, fixed-window, sliding-counter,\n"
        << "                         sliding-log, or leaky-bucket (default: token-bucket)\n"
        << "  --clients N            Distinct client IDs shared across requests (default: 100)\n"
        << "  --requests N           Total allow() calls, not per thread (default: 100000)\n"
        << "  --threads N            Concurrent worker threads (default: CPU count)\n"
        << "  --pool-size N          Redis connections; default is --threads\n"
        << "  --pool-wait-ms N       Connection-pool wait timeout (default: 1000)\n"
        << "  --capacity N           Token/leaky bucket capacity (default: 10)\n"
        << "  --refill-rate N        Token refill rate per second (default: 1)\n"
        << "  --leak-rate N          Leaky bucket drain rate per second (default: 1)\n"
        << "  --limit N              Window algorithms' limit (default: 100)\n"
        << "  --window N             Window length in seconds (default: 60)\n"
        << "  --redis-host HOST      Redis hostname (default: 127.0.0.1)\n"
        << "  --redis-port PORT      Redis port (default: 6379)\n"
        << "  --key-prefix PREFIX    Prefix used for benchmark keys (default: benchmark)\n";
}

std::string take_value(int& index, int argc, char* argv[], const std::string& option) {
    if (++index >= argc) {
        throw std::runtime_error(option + " requires a value");
    }
    return argv[index];
}

Options parse_options(int argc, char* argv[]) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            usage(argv[0]);
            std::exit(0);
        } else if (arg == "--algorithm") {
            options.algorithm = take_value(i, argc, argv, arg);
        } else if (arg == "--clients") {
            options.clients = std::stoull(take_value(i, argc, argv, arg));
        } else if (arg == "--requests") {
            options.requests = std::stoull(take_value(i, argc, argv, arg));
        } else if (arg == "--threads") {
            options.threads = std::stoull(take_value(i, argc, argv, arg));
        } else if (arg == "--pool-size") {
            options.pool_size = std::stoull(take_value(i, argc, argv, arg));
        } else if (arg == "--pool-wait-ms") {
            options.pool_wait_ms = std::stoi(take_value(i, argc, argv, arg));
        } else if (arg == "--capacity") {
            options.capacity = std::stoi(take_value(i, argc, argv, arg));
        } else if (arg == "--refill-rate") {
            options.refill_rate = std::stod(take_value(i, argc, argv, arg));
        } else if (arg == "--leak-rate") {
            options.leak_rate = std::stod(take_value(i, argc, argv, arg));
        } else if (arg == "--limit") {
            options.limit = std::stoi(take_value(i, argc, argv, arg));
        } else if (arg == "--window") {
            options.window = std::stoi(take_value(i, argc, argv, arg));
        } else if (arg == "--redis-host") {
            options.redis_host = take_value(i, argc, argv, arg);
        } else if (arg == "--redis-port") {
            options.redis_port = std::stoi(take_value(i, argc, argv, arg));
        } else if (arg == "--key-prefix") {
            options.key_prefix = take_value(i, argc, argv, arg);
        } else {
            throw std::runtime_error("Unknown option: " + arg);
        }
    }

    if (options.clients == 0 || options.requests == 0 || options.threads == 0 ||
        options.capacity <= 0 || options.limit <= 0 || options.window <= 0 ||
        options.refill_rate < 0 || options.leak_rate < 0 || options.redis_port <= 0 ||
        options.pool_wait_ms < 0) {
        throw std::runtime_error("Counts, limits, and ports must be positive; rates and wait time cannot be negative");
    }
    if (options.pool_size == 0) {
        options.pool_size = options.threads;
    }
    return options;
}

std::string lua_path(const std::string& filename) {
    // Invoke the executable from the repository root, as shown in the README.
    return "lua/" + filename;
}

std::unique_ptr<RateLimiter> make_limiter(sw::redis::Redis& redis, const Options& options) {
    if (options.algorithm == "token-bucket") {
        return std::make_unique<TokenBucket>(redis, options.capacity, options.refill_rate,
                                             lua_path("token_bucket.lua"));
    }
    if (options.algorithm == "fixed-window") {
        return std::make_unique<FixedWindow>(redis, options.limit, options.window);
    }
    if (options.algorithm == "sliding-counter") {
        return std::make_unique<SlidingWindowCounter>(redis, options.limit, options.window);
    }
    if (options.algorithm == "sliding-log") {
        return std::make_unique<SlidingWindowLog>(redis, options.limit, options.window,
                                                   lua_path("sliding_window_log.lua"));
    }
    if (options.algorithm == "leaky-bucket") {
        return std::make_unique<LeakyBucket>(redis, options.capacity, options.leak_rate,
                                              lua_path("leaky_bucket.lua"));
    }
    throw std::runtime_error("Unknown algorithm: " + options.algorithm);
}

double percentile_us(const std::vector<double>& sorted_samples, double percentile) {
    if (sorted_samples.empty()) {
        return 0.0;
    }
    const auto index = static_cast<std::size_t>(percentile * (sorted_samples.size() - 1));
    return sorted_samples[index];
}

}