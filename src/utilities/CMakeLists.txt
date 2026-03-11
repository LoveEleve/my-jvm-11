# utilities library

add_library(utilities STATIC
    ostream.cpp
)

target_include_directories(utilities PUBLIC ${CMAKE_SOURCE_DIR}/src)
