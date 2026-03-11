# test directory

# 简单的类型测试
add_executable(test_types 
    test_types.cpp
)

target_link_libraries(test_types
    utilities
    memory
    runtime
    oops
)

add_test(NAME TypesTest COMMAND test_types)

# 布局验证程序
add_executable(verify_layout
    verify_layout.cpp
)

target_link_libraries(verify_layout
    utilities
    memory
    runtime
    oops
)
