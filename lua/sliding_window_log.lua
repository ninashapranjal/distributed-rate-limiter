local key = KEYS[1]

local now = tonumber(ARGV[1])
local window = tonumber(ARGV[2])
local limit = tonumber(ARGV[3])
local requested = tonumber(ARGV[4])

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

        local request_id =
            tostring(now) .. ":" .. tostring(i) .. ":" .. tostring(math.random())

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

    return 1
end

return 0