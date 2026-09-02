local key = KEYS[1]

local limit = tonumber(ARGV[1])
local window = tonumber(ARGV[2])
local requested = tonumber(ARGV[3])

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
            window
        )
    end

    return 1
end

return 0