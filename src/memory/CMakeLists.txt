# memory library

add_library(memory STATIC
    allocation.cpp
    arena.cpp
    resourceArea.cpp
)

target_include_directories(memory PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(memory PUBLIC utilities)
