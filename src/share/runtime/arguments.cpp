#include "runtime/arguments.hpp"
#include <cstring>
#include <cstdio>

// ============================================================================
// Arguments — 命令行参数解析
// 对照 HotSpot: src/hotspot/share/runtime/arguments.cpp
// ============================================================================

// 静态成员初始化
const char* Arguments::_classpath       = ".";
const char* Arguments::_main_class_name = nullptr;
size_t      Arguments::_heap_size       = 256 * M;  // 默认 256MB
bool        Arguments::_test_mode       = false;

bool Arguments::parse(int argc, char** argv) {
    // 对照 HotSpot: Arguments::parse() [arguments.cpp:4261]
    // HotSpot 的 parse() 处理几百个参数，我们只处理核心参数

    if (argc < 2) {
        return false;
    }

    int i = 1;
    while (i < argc) {
        const char* arg = argv[i];

        // --test: 运行回归测试模式
        if (strcmp(arg, "--test") == 0) {
            _test_mode = true;
            i++;
            continue;
        }

        // -cp <path> 或 -classpath <path>
        // 对照: Arguments 中 -Djava.class.path 的处理
        if (strcmp(arg, "-cp") == 0 || strcmp(arg, "-classpath") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: %s requires a path argument\n", arg);
                return false;
            }
            _classpath = argv[i + 1];
            i += 2;
            continue;
        }

        // -Xmx<size>: 最大堆大小
        // 对照: Arguments::parse_xmx() (简化)
        if (strncmp(arg, "-Xmx", 4) == 0) {
            _heap_size = parse_size(arg + 4);
            if (_heap_size == 0) {
                fprintf(stderr, "Error: Invalid heap size: %s\n", arg);
                return false;
            }
            i++;
            continue;
        }

        // 未知的 - 开头参数
        if (arg[0] == '-') {
            fprintf(stderr, "Error: Unrecognized option: %s\n", arg);
            return false;
        }

        // 非 - 开头的参数 → 主类名（最后一个非选项参数）
        _main_class_name = arg;
        i++;
    }

    // 非测试模式下必须有主类名
    if (!_test_mode && _main_class_name == nullptr) {
        return false;
    }

    return true;
}

size_t Arguments::parse_size(const char* str) {
    char* end = nullptr;
    size_t value = (size_t)strtoul(str, &end, 10);

    if (end == str) return 0;  // 没有数字

    // 处理后缀
    if (*end == 'k' || *end == 'K') {
        value *= K;
    } else if (*end == 'm' || *end == 'M') {
        value *= M;
    } else if (*end == 'g' || *end == 'G') {
        value *= G;
    } else if (*end != '\0') {
        return 0;  // 无效后缀
    }

    return value;
}

void Arguments::print_usage() {
    fprintf(stderr,
        "Usage: mini_jvm [options] <mainclass>\n"
        "\n"
        "Options:\n"
        "  -cp <path>        Set classpath (default: .)\n"
        "  -classpath <path> Set classpath (default: .)\n"
        "  -Xmx<size>        Set maximum heap size (e.g., 256m, 1g)\n"
        "  --test             Run regression tests\n"
        "\n"
        "Examples:\n"
        "  mini_jvm -cp test HelloWorld\n"
        "  mini_jvm -cp classes com/example/Main\n"
        "  mini_jvm -Xmx512m -cp . MyApp\n"
        "  mini_jvm --test\n"
    );
}
