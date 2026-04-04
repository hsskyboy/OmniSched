cmake_minimum_required(VERSION 3.30.1)
project(zygisk_omnisched CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(COMMON_CXX_FLAGS "-Os -flto -fvisibility=hidden -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_CXX_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections -s -flto")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections -s -flto")

include_directories(include)

include(FetchContent)
FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(json)

set(OMNISCHED_DAEMON_SOURCES
    src/config.cpp
    src/core_sched.cpp
    src/main.cpp
    src/root_adapter.cpp
    src/utils.cpp
)

set(OMNISCHED_ZYGISK_SOURCES
    src/config.cpp
    src/utils.cpp
    src/zygisk_core.cpp
)

add_executable(omnisched_daemon ${OMNISCHED_DAEMON_SOURCES})
target_link_libraries(omnisched_daemon PRIVATE nlohmann_json::nlohmann_json)
set_target_properties(omnisched_daemon PROPERTIES OUTPUT_NAME "omnisched")

add_library(zygisk_omnisched SHARED ${OMNISCHED_ZYGISK_SOURCES})
target_link_libraries(zygisk_omnisched PRIVATE nlohmann_json::nlohmann_json)
set_target_properties(zygisk_omnisched PROPERTIES PREFIX "" OUTPUT_NAME "arm64-v8a")
