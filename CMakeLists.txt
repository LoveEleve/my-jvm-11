cmake_minimum_required(VERSION 3.10)
project(my_jvm)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# 头文件目录
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/utilities
    ${CMAKE_SOURCE_DIR}/src/runtime
    ${CMAKE_SOURCE_DIR}/src/oops
    ${CMAKE_SOURCE_DIR}/src/memory
    ${CMAKE_SOURCE_DIR}/src/classfile
    ${CMAKE_SOURCE_DIR}/src/interpreter
    ${CMAKE_SOURCE_DIR}/src/thread
    ${CMAKE_SOURCE_DIR}/src/gc
)

# 源文件目录
add_subdirectory(src)

# 测试
enable_testing()
add_subdirectory(test)

# 打印配置信息
message(STATUS "=== my_jvm Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "=================================")
