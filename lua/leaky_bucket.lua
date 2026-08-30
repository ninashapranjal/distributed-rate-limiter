local key = KEYS[1]

local capacity = tonumber(ARGV[1])
local leak_rate = tonumber(ARGV[2])
local now = tonumber(ARGV[3])
local requested = tonumber(ARGV[4])

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
redis.call(
    "EXPIRE",
    key,
    math.ceil(capacity / leak_rate) + 1
)

return allowed