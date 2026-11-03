cmake_minimum_required(VERSION 3.10)
project(WeatherStation)

set(CMAKE_CXX_STANDARD 17)

# Find and link threads required by cpp-httplib
find_package(Threads REQUIRED)

include_directories(${CMAKE_SOURCE_DIR})

add_executable(weather_server main.cpp)
target_link_libraries(weather_server Threads::Threads)
