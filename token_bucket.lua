local key = KEYS[1]

local capacity = tonumber(ARGV[1])
local refill_rate = tonumber(ARGV[2])
local now = tonumber(ARGV[3])
local requested = tonumber(ARGV[4])

--current bucket state
local data = redis.call(
    "HMGET",
    key,
    "tokens",
    "last_refill"
)

local tokens = tonumber(data[1])
local last_refill = tonumber(data[2])

if tokens == nil then
    tokens = capacity
    last_refill = now
end

--time passed
local elapsed = now - last_refill

tokens = tokens + (elapsed * refill_rate)

if tokens > capacity then
    tokens = capacity
end

local allowed = 0

if tokens >= requested then
    tokens = tokens - requested
    allowed = 1
end

-- save updated bucket state
redis.call(
    "HSET",
    key,
    "tokens",
    tokens,
    "last_refill",
    now
)

return {
    allowed,
    tokens
}