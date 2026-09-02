#include <algorithm>
#include <string>


//benchmarking configuration
struct Options{
  std::string algorithm = "token-bucket"; //default
  std::size_t clients = 100;
  std::size_t clients = 100;
  std::size_t requests = 100000;
  std::size_t pool_size = 0;
  int pool_wait_ms = 1000;
  int capacity = 10;
  double refill_rate = 1.0;
  int limit = 100;
  int window = 60;
  double leak_rate = 1.0;
  std::string redis_host = "127.0.0.1";
  int redis_port = 6379;
}
