cmake_minimum_required(VERSION 3.30.1)
project(OmniSchedV2 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -flto -fvisibility=hidden -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections -s -flto")

include_directories(include)

file(GLOB SOURCES "src/*.cpp")

add_executable(omnisched ${SOURCES})
