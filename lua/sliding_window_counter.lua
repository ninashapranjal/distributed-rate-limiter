local current_count =
    tonumber(redis.call("GET", KEYS[1])) or 0

local previous_count =
    tonumber(redis.call("GET", KEYS[2])) or 0

local elapsed =
    tonumber(ARGV[1])

local window =
    tonumber(ARGV[2])

local requested =
    tonumber(ARGV[3])

local limit =
    tonumber(ARGV[4])

-- how much of the prev window
-- should contribute to the curr window
local previous_weight =
    (window - elapsed) / window

-- estimate the no of requests
-- currently inside the sliding window.
local estimated_count =
    previous_count * previous_weight
    + current_count

-- check whether request fits
if estimated_count + requested <= limit then

    redis.call(
        "INCRBY",
        KEYS[1],
        requested
    )

    -- keep curr window key alive for one complete window
    redis.call(
        "EXPIRE",
        KEYS[1],
        window
    )

    return 1
end

return 0