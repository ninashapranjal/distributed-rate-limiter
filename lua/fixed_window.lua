local base_key = KEYS[1]

local requested = tonumber(ARGV[1])
local window = tonumber(ARGV[2])
local limit = tonumber(ARGV[3])

local time = redis.call("TIME")
local now = tonumber(time[1])
local window_id = math.floor(now / window)
local key = base_key .. ":" .. window_id
local count = tonumber(redis.call("GET", key)) or 0

if count + requested <= limit then

    local new_count = redis.call(
        "INCRBY",
        key,
        requested
    )

    if count == 0 then
        redis.call(
            "EXPIRE",
            key,
            window - (now % window)
        )
    end

    return 1
end

return 0
