# Distributed Rate Limiter
Implementing rate-limiting algorithms in C++ with Redis and Lua.

## Algorithms
- Token Bucket - implemented
- Leaky Bucket - planned
- Fixed Window Counter - planned
- Sliding Window Log - planned
- Sliding Window Counter - planned

## Overview
....

## Token Bucket
Tokens are added to a bucket at a fixed rate until the bucket's maximum capacity is reached. Each request consumes a specified number of tokens. Requests are allowed when sufficient tokens are available. Requests are rejected when insufficient tokens are available.

### Current Configuration
- Bucket capacity: 10 tokens
- Refill rate: 1 token/second
- Cost per request: 1 token
