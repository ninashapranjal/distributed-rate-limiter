cmake_minimum_required(VERSION 3.16)

project(distributed_rate_limiter)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(rate_limiter
    main.cpp
    src/Config.cpp
    src/TokenBucket.cpp
    src/FixedWindow.cpp
)

target_include_directories(rate_limiter PRIVATE
    include
)

find_package(redis++ REQUIRED)

target_link_libraries(rate_limiter PRIVATE
    redis++::redis++
)