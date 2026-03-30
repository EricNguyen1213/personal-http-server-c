cmake_minimum_required(VERSION 3.13)
project(codecrafters-http-server)

set(CMAKE_C_STANDARD 23)



file(GLOB_RECURSE SOURCE_FILES src/*.c src/*.h)
add_executable(http-server ${SOURCE_FILES})




