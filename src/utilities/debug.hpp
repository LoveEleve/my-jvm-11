/*
 * my_jvm - Debug assertions
 * 
 * 参考 OpenJDK 11 hotspot/src/hotspot/share/utilities/debug.hpp
 * 简化版本，只保留核心断言宏
 */

#ifndef MY_JVM_UTILITIES_DEBUG_HPP
#define MY_JVM_UTILITIES_DEBUG_HPP

#include "utilities/breakpoint.hpp"
#include "utilities/compilerWarnings.hpp"
#include "utilities/macros.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstdarg>

// ========== 断言控制 ==========

#ifndef ASSERT
#ifdef DEBUG
#define ASSERT 1
#else
#define ASSERT 0
#endif
#endif

// ========== 错误报告辅助函数 ==========

inline void report_vm_error(const char* file, int line, const char* error_msg) {
  fprintf(stderr, "ERROR: %s:%d: %s\n", file, line, error_msg);
  std::abort();
}

inline void report_vm_error(const char* file, int line, const char* error_msg, 
                           const char* detail_fmt, ...) ATTRIBUTE_PRINTF(4, 5);
inline void report_vm_error(const char* file, int line, const char* error_msg, 
                           const char* detail_fmt, ...) {
  va_list args;
  va_start(args, detail_fmt);
  fprintf(stderr, "ERROR: %s:%d: %s - ", file, line, error_msg);
  vfprintf(stderr, detail_fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
  std::abort();
}

// ========== 断言宏 ==========

#if ASSERT

#define vmassert(p, ...)                                                       \
do {                                                                           \
  if (!(p)) {                                                                  \
    report_vm_error(__FILE__, __LINE__, "assert(" #p ") failed", __VA_ARGS__); \
    BREAKPOINT;                                                                \
  }                                                                            \
} while (0)

#define vmassert_status(p, status, msg)                                        \
do {                                                                           \
  if (!(p)) {                                                                  \
    report_vm_error(__FILE__, __LINE__, "assert(" #p ") failed", msg);         \
    BREAKPOINT;                                                                \
  }                                                                            \
} while (0)

#else // ASSERT

#define vmassert(p, ...)
#define vmassert_status(p, status, msg)

#endif // ASSERT

// 兼容性别名
#define assert(p, ...) vmassert(p, __VA_ARGS__)
#define assert_status(p, status, msg) vmassert_status(p, status, msg)

// ========== guarantee 宏（始终执行） ==========

#define guarantee(p, ...)                                                      \
do {                                                                           \
  if (!(p)) {                                                                  \
    report_vm_error(__FILE__, __LINE__, "guarantee(" #p ") failed", __VA_ARGS__); \
    BREAKPOINT;                                                                \
  }                                                                            \
} while (0)

// ========== 致命错误宏 ==========

#define fatal(...)                                                             \
do {                                                                           \
  report_vm_error(__FILE__, __LINE__, "fatal error", __VA_ARGS__);             \
  BREAKPOINT;                                                                  \
} while (0)

// ========== 不应到达标记 ==========

#define ShouldNotCallThis()                                                    \
do {                                                                           \
  report_vm_error(__FILE__, __LINE__, "ShouldNotCallThis()");                  \
  BREAKPOINT;                                                                  \
} while (0)

#define ShouldNotReachHere()                                                   \
do {                                                                           \
  report_vm_error(__FILE__, __LINE__, "ShouldNotReachHere()");                 \
  BREAKPOINT;                                                                  \
} while (0)

#define Unimplemented()                                                        \
do {                                                                           \
  report_vm_error(__FILE__, __LINE__, "Unimplemented()");                      \
  BREAKPOINT;                                                                  \
} while (0)

// ========== 编译期断言 ==========

template <bool cond> struct STATIC_ASSERT_FAILURE;
template <> struct STATIC_ASSERT_FAILURE<true> { enum { value = 1 }; };

#define STATIC_ASSERT(cond)                                                    \
  (void)STATIC_ASSERT_FAILURE<cond>::value

// ========== 预条件/后条件 ==========

#define precond(p)   assert(p, "precond")
#define postcond(p)  assert(p, "postcond")

// ========== 警告输出 ==========

inline void warning(const char* format, ...) ATTRIBUTE_PRINTF(1, 2);
inline void warning(const char* format, ...) {
  va_list args;
  va_start(args, format);
  fprintf(stderr, "WARNING: ");
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
  va_end(args);
}

#endif // MY_JVM_UTILITIES_DEBUG_HPP
