--lua arrays start at 1
local key = KEYS[1] -- client_id

local capacity = tonumber(ARGV[1])
local refill_rate = tonumber(ARGV[2])
local now = tonumber(ARGV[3])
local requested = tonumber(ARGV[4])

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

return {allowed, tokens}