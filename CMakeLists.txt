cmake_minimum_required(VERSION 3.13)
project(codecrafters-http-server)

set(CMAKE_C_STANDARD 23)

# 1. Find both packages
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

file(GLOB_RECURSE SOURCE_FILES src/*.c src/*.h)
add_executable(http-server ${SOURCE_FILES})

# 2. Link them both to your target
target_link_libraries(http-server PRIVATE 
    ZLIB::ZLIB 
    Threads::Threads
)


