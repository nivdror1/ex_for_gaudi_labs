cmake_minimum_required(VERSION 3.3.2)
project(gaudiLabs)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11++")

set(SOURCE_FILES MemoryManager.cpp)
add_executable(gaudiLabs ${SOURCE_FILES})