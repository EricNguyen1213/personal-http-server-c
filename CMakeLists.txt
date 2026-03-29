cmake_minimum_required(VERSION 3.13)
project(codecrafters-http-server)

set(CMAKE_C_STANDARD 23)

# 1. Use PkgConfig (vcpkg will route this to its own internal folders)
find_package(PkgConfig REQUIRED)

# 2. Specifically look for glib-2.0
pkg_check_modules(GLIB REQUIRED glib-2.0)

file(GLOB_RECURSE SOURCE_FILES src/*.c src/*.h)
add_executable(http-server ${SOURCE_FILES})

# 3. Use the variables PkgConfig generated
target_link_libraries(http-server PRIVATE ${GLIB_LIBRARIES})
target_include_directories(http-server PRIVATE ${GLIB_INCLUDE_DIRS})



