/*
 * my_jvm - Compiler warnings control
 * 
 * 参考 OpenJDK 11 hotspot/src/hotspot/share/utilities/compilerWarnings.hpp
 * 简化版本，只保留 GCC/Clang 相关宏
 */

#ifndef MY_JVM_UTILITIES_COMPILERWARNINGS_HPP
#define MY_JVM_UTILITIES_COMPILERWARNINGS_HPP

// ========== GCC/Clang 编译器相关宏 ==========

#if defined(__GNUC__) || defined(__clang__)

// 禁用 GCC 警告的辅助宏
#define PRAGMA_DISABLE_GCC_WARNING_AUX(x) _Pragma(#x)
#define PRAGMA_DISABLE_GCC_WARNING(option_string) \
  PRAGMA_DISABLE_GCC_WARNING_AUX(GCC diagnostic ignored option_string)

// printf/scanf 格式属性（GCC 4.2+）
#if ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 2)) || (__GNUC__ > 4) || defined(__clang__)
#ifndef ATTRIBUTE_PRINTF
#define ATTRIBUTE_PRINTF(fmt, vargs)  __attribute__((format(printf, fmt, vargs)))
#endif
#ifndef ATTRIBUTE_SCANF
#define ATTRIBUTE_SCANF(fmt, vargs)  __attribute__((format(scanf, fmt, vargs)))
#endif
#else
#define ATTRIBUTE_PRINTF(fmt, vargs)
#define ATTRIBUTE_SCANF(fmt, vargs)
#endif

// 忽略格式警告
#define PRAGMA_FORMAT_NONLITERAL_IGNORED \
  _Pragma("GCC diagnostic ignored \"-Wformat-nonliteral\"") \
  _Pragma("GCC diagnostic ignored \"-Wformat-security\"")
#define PRAGMA_FORMAT_IGNORED _Pragma("GCC diagnostic ignored \"-Wformat\"")

// GCC 8+ 的警告禁用
#if !defined(__clang__) && (__GNUC__ >= 8)
#define PRAGMA_STRINGOP_TRUNCATION_IGNORED PRAGMA_DISABLE_GCC_WARNING("-Wstringop-truncation")
#define PRAGMA_MAYBE_UNINITIALIZED_IGNORED PRAGMA_DISABLE_GCC_WARNING("-Wmaybe-uninitialized")
#else
#define PRAGMA_STRINGOP_TRUNCATION_IGNORED
#define PRAGMA_MAYBE_UNINITIALIZED_IGNORED
#endif

// 诊断 push/pop (GCC 4.6+, Clang 3.1+)
#if defined(__clang__) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 6)) || (__GNUC__ > 4)
#define PRAGMA_DIAG_PUSH _Pragma("GCC diagnostic push")
#define PRAGMA_DIAG_POP  _Pragma("GCC diagnostic pop")
#else
#define PRAGMA_DIAG_PUSH
#define PRAGMA_DIAG_POP
#endif

#else
// ========== 非 GCC 编译器默认值 ==========

#define ATTRIBUTE_PRINTF(fmt, vargs)
#define ATTRIBUTE_SCANF(fmt, vargs)
#define PRAGMA_FORMAT_NONLITERAL_IGNORED
#define PRAGMA_FORMAT_IGNORED
#define PRAGMA_STRINGOP_TRUNCATION_IGNORED
#define PRAGMA_MAYBE_UNINITIALIZED_IGNORED
#define PRAGMA_DIAG_PUSH
#define PRAGMA_DIAG_POP

#endif // __GNUC__ || __clang__

#endif // MY_JVM_UTILITIES_COMPILERWARNINGS_HPP
