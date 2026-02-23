#ifndef SHARE_UTILITIES_DEBUG_HPP
#define SHARE_UTILITIES_DEBUG_HPP

// ============================================================================
// Mini JVM - 断言与错误报告
// 对应 HotSpot: src/hotspot/share/utilities/debug.hpp
//
// HotSpot 有复杂的错误报告系统（生成 hs_err_pid.log 等），
// 我们精简为：打印错误信息 + abort()
// ============================================================================

#include <cstdio>
#include <cstdlib>

// ----------------------------------------------------------------------------
// VMErrorType
// HotSpot: debug.hpp 约 152-158 行
// ----------------------------------------------------------------------------

enum VMErrorType {
    INTERNAL_ERROR      = 0xe0000000,
    OOM_MALLOC_ERROR    = 0xe0000001,
    OOM_MMAP_ERROR      = 0xe0000002,
    OOM_MPROTECT_ERROR  = 0xe0000003,
    OOM_JAVA_HEAP_FATAL = 0xe0000004
};

// ----------------------------------------------------------------------------
// 错误报告函数
// HotSpot 版本会生成 hs_err_pid.log，我们只打印到 stderr
// ----------------------------------------------------------------------------

inline void report_vm_error(const char* file, int line, const char* msg) {
    fprintf(stderr, "\n# A fatal error has been detected by the Mini JVM:\n");
    fprintf(stderr, "#\n#  %s\n", msg);
    fprintf(stderr, "#  at %s:%d\n", file, line);
    fprintf(stderr, "#\n");
    fflush(stderr);
    abort();
}

inline void report_vm_error(const char* file, int line, const char* msg,
                            const char* detail) {
    fprintf(stderr, "\n# A fatal error has been detected by the Mini JVM:\n");
    fprintf(stderr, "#\n#  %s\n", msg);
    fprintf(stderr, "#  %s\n", detail);
    fprintf(stderr, "#  at %s:%d\n", file, line);
    fprintf(stderr, "#\n");
    fflush(stderr);
    abort();
}

// ----------------------------------------------------------------------------
// 断言宏
// HotSpot: debug.hpp 约 54-107 行
//
// assert(p, msg)    — 仅 Debug 构建生效
// guarantee(p, msg) — 始终生效
// fatal(msg)        — 无条件报错
// ----------------------------------------------------------------------------

// assert：仅 ASSERT 模式下生效
#ifdef ASSERT
#define vm_assert(p, msg)                                           \
    do {                                                            \
        if (!(p)) {                                                 \
            report_vm_error(__FILE__, __LINE__,                     \
                "assert(" #p ") failed", msg);                      \
        }                                                           \
    } while (0)
#else
#define vm_assert(p, msg)  do {} while(0)
#endif

// guarantee：始终生效（用于关键检查）
#define guarantee(p, msg)                                           \
    do {                                                            \
        if (!(p)) {                                                 \
            report_vm_error(__FILE__, __LINE__,                     \
                "guarantee(" #p ") failed", msg);                   \
        }                                                           \
    } while (0)

// fatal：无条件报错
#define fatal(msg)                                                  \
    do {                                                            \
        report_vm_error(__FILE__, __LINE__, "fatal error", msg);    \
    } while (0)

// 不应该调用/不应该到达/未实现
#define ShouldNotCallThis()                                         \
    report_vm_error(__FILE__, __LINE__, "ShouldNotCallThis()")

#define ShouldNotReachHere()                                        \
    report_vm_error(__FILE__, __LINE__, "ShouldNotReachHere()")

#define Unimplemented()                                             \
    report_vm_error(__FILE__, __LINE__, "Unimplemented()")

// warning：非致命警告
inline void warning(const char* format, ...) {
    fprintf(stderr, "WARNING: ");
    // 简化版，只打印 format 字符串
    fprintf(stderr, "%s\n", format);
}

#endif // SHARE_UTILITIES_DEBUG_HPP
