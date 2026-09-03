local base_key = KEYS[1]
local window = tonumber(ARGV[1])
local requested = tonumber(ARGV[2])
local limit = tonumber(ARGV[3])
local time = redis.call("TIME")
local now = tonumber(time[1]) + tonumber(time[2]) / 1000000
local current_window = math.floor(now / window)
local elapsed = now - current_window * window
local current_key = base_key .. ":" .. current_window
local previous_key = base_key .. ":" .. (current_window - 1)
local current_count = tonumber(redis.call("GET", current_key)) or 0
local previous_count = tonumber(redis.call("GET", previous_key)) or 0

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
        current_key,
        requested
    )

    -- It is needed throughout the next window as the previous bucket.
    if current_count == 0 then
        redis.call("EXPIRE", current_key, window * 2)
    end

    return 1
end

return 0
