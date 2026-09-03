local key = KEYS[1]

local capacity = tonumber(ARGV[1])
local leak_rate = tonumber(ARGV[2])
local requested = tonumber(ARGV[3])
local redis_time = redis.call("TIME")
local now = tonumber(redis_time[1]) + tonumber(redis_time[2]) / 1000000

-- get current bucket state
local data = redis.call(
    "HMGET",
    key,
    "level",
    "last_update"
)

local level = tonumber(data[1])
local last_update = tonumber(data[2])

if level == nil then
    level = 0
    last_update = now
end

-- calculate how much has leaked since last update
local elapsed = now - last_update
if elapsed < 0 then
    elapsed = 0
end

local leaked = elapsed * leak_rate

level = level - leaked

if level < 0 then
    level = 0
end

-- check if request fits
local allowed = 0

if level + requested <= capacity then
    level = level + requested
    allowed = 1
end

-- update 
redis.call(
    "HSET",
    key,
    "level",
    level,
    "last_update",
    now
)

-- keep key alive
-- with no leak the level must persist, otherwise expiration would incorrectly
-- reset a permanently full bucket. A positive leak can expire once fully empty.
if leak_rate > 0 then
    redis.call("EXPIRE", key, math.ceil(capacity / leak_rate) + 1)
end

return allowed
