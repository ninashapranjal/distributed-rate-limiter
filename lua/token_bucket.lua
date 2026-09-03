--lua arrays start at 1
local key = KEYS[1] -- client_id

local capacity = tonumber(ARGV[1])
local refill_rate = tonumber(ARGV[2])
local requested = tonumber(ARGV[3])

-- redis is the single authority for time. This avoids different benchmark
-- processes (or hosts) making inconsistent refill decisions because of clock skew.
local redis_time = redis.call("TIME")
local now = tonumber(redis_time[1]) + tonumber(redis_time[2]) / 1000000

-- geting the current bucket state
local data = redis.call(
    "HMGET", -- gets the values of multiple given fields from a hash stored at key
    key,
    "tokens", -- no of tokens currently left in bucket
    "last_refill" -- timestamp of when the bucket was last refilled
)

local tokens = tonumber(data[1])
local last_refill = tonumber(data[2])

-- initialize full bucket
if tokens == nil then
    tokens = capacity
    last_refill = now
end

-- how much time has passed since last request
local elapsed = now - last_refill
if elapsed < 0 then
    elapsed = 0
end

tokens = tokens + (elapsed * refill_rate)

if tokens > capacity then
    tokens = capacity
end

-- can request be done?
local allowed = 0

if tokens >= requested then
    tokens = tokens - requested
    allowed = 1
end

-- update
redis.call(
    "HSET",
    key,
    "tokens",
    tokens,
    "last_refill",
    now -- current timestamp
)

-- A bucket needs state only until it is full. Zero-refill buckets deliberately
-- retain their state, since they must not become full again after eviction.
if refill_rate > 0 then
    redis.call("EXPIRE", key, math.ceil(capacity / refill_rate) + 1)
end

return allowed
