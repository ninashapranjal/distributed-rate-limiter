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
class StartGate {
  public:
    explicit StartGate(std::size_t participants) : participants_(participants) {}

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
