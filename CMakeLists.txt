cmake_minimum_required(VERSION 3.10)
project(mini_jvm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Debug 模式启用断言
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG -DASSERT)
endif()

# Linux 平台定义
if(UNIX AND NOT APPLE)
    add_definitions(-DLINUX -DAMD64 -D_LP64)
endif()

# 头文件搜索路径
include_directories(
    ${CMAKE_SOURCE_DIR}/src/share
    ${CMAKE_SOURCE_DIR}/src/os/linux
    ${CMAKE_SOURCE_DIR}/src/cpu/x86
)

# 收集源文件（不含入口文件）
file(GLOB_RECURSE LIB_SOURCES
    "src/share/*.cpp"
    "src/os/linux/*.cpp"
    "src/cpu/x86/*.cpp"
)

# ═══ 主程序：真正的 JVM 入口 ═══
# 入口: src/main.cpp
# 回归测试: test/main.cpp（通过 --test 参数触发）
add_executable(mini_jvm
    ${LIB_SOURCES}
    src/main.cpp
    test/main.cpp
)

target_link_libraries(mini_jvm pthread dl)
