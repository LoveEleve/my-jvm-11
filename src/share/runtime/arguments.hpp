#ifndef SHARE_RUNTIME_ARGUMENTS_HPP
#define SHARE_RUNTIME_ARGUMENTS_HPP

// ============================================================================
// Mini JVM - Arguments（命令行参数解析）
// 对照 HotSpot: src/hotspot/share/runtime/arguments.hpp
//
// HotSpot 的 Arguments 类负责:
//   - 解析 -Xms / -Xmx / -XX: 等 VM 参数
//   - 自动调优 (Ergonomics)
//   - 系统属性管理
//
// 我们的简化版只支持:
//   -cp <classpath>  或  -classpath <classpath>
//   -Xmx<size>       堆大小（可选，默认 256MB）
//   <mainclass>      主类名
//
// 用法: ./mini_jvm [-cp <path>] [-Xmx<size>] <mainclass>
// ============================================================================

#include "utilities/globalDefinitions.hpp"

class Arguments {
public:
    // 解析命令行参数
    // 对照: Arguments::parse() [arguments.cpp:4261]
    // 返回 true 表示解析成功
    static bool parse(int argc, char** argv);

    // 打印用法信息
    static void print_usage();

    // ======== 参数值访问 ========

    // -cp / -classpath 的值（默认 "."）
    static const char* classpath()       { return _classpath; }

    // 主类名（如 "HelloWorld" 或 "com/example/Main"）
    static const char* main_class_name() { return _main_class_name; }

    // -Xmx 堆大小（字节，默认 256MB）
    static size_t heap_size()            { return _heap_size; }

    // 是否为运行测试模式（--test 参数）
    static bool is_test_mode()           { return _test_mode; }

private:
    static const char* _classpath;
    static const char* _main_class_name;
    static size_t      _heap_size;
    static bool        _test_mode;

    // 解析 -Xmx 的大小值（支持 k/m/g 后缀）
    static size_t parse_size(const char* str);
};

#endif // SHARE_RUNTIME_ARGUMENTS_HPP
