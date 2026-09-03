local key = KEYS[1]

local sequence_key = KEYS[2]
local window = tonumber(ARGV[1])
local limit = tonumber(ARGV[2])
local requested = tonumber(ARGV[3])
local time = redis.call("TIME")
local now = tonumber(time[1]) + tonumber(time[2]) / 1000000

local window_start = now - window

-- remove requests that are outside the sliding window
redis.call(
    "ZREMRANGEBYSCORE",
    key,
    0,
    window_start
)

-- count requests currently inside the window
local current_count = redis.call(
    "ZCARD",
    key
)

-- check whether the request can be accepted
if current_count + requested <= limit then

    -- add requets
    for i = 1, requested do

        -- redis serializes Lua execution, and INCR creates a collision-free
        -- member ID even when TIME returns the same microsecond twice.
        local request_id = tostring(now) .. ":" .. redis.call("INCR", sequence_key)

        redis.call(
            "ZADD",
            key,
            now,
            request_id
        )

    end

    -- remove the key after the window
    redis.call(
        "EXPIRE",
        key,
        window
    )
    redis.call("EXPIRE", sequence_key, window)

    return 1
end

return 0
