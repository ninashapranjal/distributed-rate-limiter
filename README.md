# Distributed Rate Limiter

Implementing common rate-limiting algorithms with Redis. Each
request is accepted or rejected by an atomic Redis Lua operation, which makes the
limiters safe to use from multiple processes or threads at the same time.

## What is included

| Algorithm | Status | Best for |
| --- | --- | --- |
| Token bucket | Implemented | Allowing short bursts while enforcing an average rate |
| Leaky bucket | Implemented | Smoothing traffic to a steady output rate |
| Fixed window counter | Implemented | Simple, low-memory limits such as 100 requests per minute |
| Sliding window log | Implemented | An exact rolling-window limit where accuracy matters most |
| Sliding window counter | Implemented | A low-memory rolling-window approximation |

## How it works

Every limiter stores its state in Redis. The read, admission decision, state
update, and expiry handling happen within one Lua execution on the Redis server.
That means concurrent clients cannot slip between separate read and write calls,
and all decisions use Redis server time instead of each application's local clock.

The implementations validate invalid limits and non-positive request costs before
they reach Redis.

### Choosing an algorithm

- **Token bucket** refills tokens at a fixed rate up to a capacity. A request is
  allowed only when enough tokens remain. Choose it when brief bursts are OK.
- **Leaky bucket** tracks a bucket level that drains continuously. It smooths
  admitted work; it is not a background worker or a job queue.
- **Fixed window counter** counts requests in fixed intervals (for example, each
  minute). It is compact and simple, but requests clustered at a boundary can
  briefly exceed the intended rolling rate.
- **Sliding window log** keeps timestamps for admitted requests and checks the
  exact preceding window. It gives a strict rolling limit at the cost of more
  Redis memory.
- **Sliding window counter** combines the current fixed-window count with a
  weighted portion of the previous one. It uses less memory than a log, but is an
  approximation and is not suitable when a strict rolling cap is required.

## Prerequisites

- CMake 3.16 or later
- A C++17 compiler
- Redis running locally or reachable over the network
- [redis-plus-plus](https://github.com/sewenew/redis-plus-plus) available to CMake
  (and its hiredis dependency)

The default Redis address is `tcp://127.0.0.1:6379`.

## Build

From the repository root:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

On a single-configuration generator, omit `--config Release`.

## Run the example

`rate_limiter` sends 15 requests for one client and prints whether each request
was allowed or blocked.

```powershell
Push-Location build
.\Release\rate_limiter.exe --algorithm token-bucket --client alice --capacity 10 --refill-rate 1
Pop-Location
```

Run this executable from the `build` directory so it can locate the Lua scripts
under the repository's `lua` directory. On a single-configuration generator, use
`.\rate_limiter.exe` rather than `.\Release\rate_limiter.exe`.

Available algorithms are `token-bucket`, `leaky-bucket`, `fixed-window`,
`sliding-log`, and `sliding-counter`.

### Current default configuration

If you run `rate_limiter` without arguments, it uses the following token-bucket
configuration:

| Setting | Default |
| --- | --- |
| Algorithm | `token-bucket` |
| Client ID | `user1` |
| Redis URL | `tcp://127.0.0.1:6379` |
| Bucket capacity | `10` tokens |
| Refill rate | `1` token/second |
| Leaky-bucket drain rate | `1` unit/second |
| Window-algorithm limit | `100` requests |
| Window length | `60` seconds |

The example executable sends 15 requests, 100 ms apart. With the defaults, the
initial ten requests are allowed; later requests are admitted only as tokens
refill.

### Configuration reference

| Option | Used by | Meaning | Default |
| --- | --- | --- | --- |
| `--algorithm` | All | Limiter to run | `token-bucket` |
| `--client` | All | Client identifier to limit | `user1` |
| `--redis-url` | All | Redis connection URL | `tcp://127.0.0.1:6379` |
| `--capacity` | Token/leaky bucket | Maximum bucket size | `10` |
| `--refill-rate` | Token bucket | Tokens added per second | `1` |
| `--leak-rate` | Leaky bucket | Units drained per second | `1` |
| `--limit` | Window algorithms | Maximum requests in a window | `100` |
| `--window` | Window algorithms | Window length in seconds | `60` |

Examples:

```powershell
# At most 100 requests per 60-second fixed window
.\build\Release\rate_limiter.exe --algorithm fixed-window --client alice --limit 100 --window 60

# A strict rolling 60-second limit
.\build\Release\rate_limiter.exe --algorithm sliding-log --client alice --limit 100 --window 60

# A bucket that drains at two units per second
.\build\Release\rate_limiter.exe --algorithm leaky-bucket --client alice --capacity 20 --leak-rate 2
```

For the script-backed token, leaky, and sliding-log examples, use the same
`Push-Location build` pattern shown above.

## Benchmarking

The `benchmark` executable measures throughput, average latency, p50, p95, and
p99 `allow()` latency. It also reports allowed, rejected, and failed requests.

```powershell
cmake --build build --config Release --target benchmark
.\build\Release\benchmark.exe --algorithm token-bucket --clients 100 --requests 100000 --threads 8
```

`--clients` controls the number of distinct Redis keys. A small value deliberately
creates contention; a larger value spreads the requests across clients. Connections
are pooled, with one connection per worker by default. Compare pool sizes with:

```powershell
.\build\Release\benchmark.exe --algorithm token-bucket --clients 100 --requests 100000 --threads 8 --pool-size 1
.\build\Release\benchmark.exe --algorithm token-bucket --clients 100 --requests 100000 --threads 8 --pool-size 8
```

Use a unique `--key-prefix` for independent runs, or delete the matching Redis
keys between runs. Run `benchmark --help` for every benchmarking option.

### Reproducible benchmark matrix

Run the complete five-algorithm matrix with three repetitions at 1, 2, 4, 8, 16,
and 32 threads:

```powershell
.\scripts\run_benchmark_matrix.ps1 -Benchmark .\build\Release\benchmark.exe
```

The runner writes a timestamped directory under `results` containing:

- `raw-results.csv` — every benchmark run
- `summary.csv` — averages for each configuration
- `throughput.png` and `p99-latency.png` — summary charts

Each sample receives a unique Redis key prefix. The default matrix uses a limit or
capacity at least as large as the number of requests, so it measures the admission
path rather than mostly measuring expected rejections. For a cross-platform runner
that creates SVG charts, use `python scripts/run_benchmark_matrix.py --help`.

## Correctness notes

- Token and leaky bucket state supports fractional refill or drain values and
  expires only when it can safely return to an empty/full-equivalent state.
- Fixed-window keys expire at the next shared Redis window boundary, not a full
  window after the first request.
- The sliding-window log assigns each admitted unit a Redis-generated sequence
  number, preventing simultaneous requests from overwriting each other.
- Sliding-window-counter data remains available through the next window so its
  previous-window weight is calculated correctly.