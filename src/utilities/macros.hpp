/*
 * my_jvm - OpenJDK 11 inspired utility macros
 * 
 * 基础宏定义，来自 OpenJDK 11 hotspot/src/share/vm/utilities/macros.hpp
 */

#ifndef MY_JVM_UTILITIES_MACROS_HPP
#define MY_JVM_UTILITIES_MACROS_HPP

// ========== 禁用拷贝/赋值宏 ==========

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&) = delete; \
    void operator=(const TypeName&) = delete

// ========== 编译器相关宏 ==========

#define MY_JVM_LIKELY(x)   __builtin_expect(!!(x), 1)
#define MY_JVM_UNLIKELY(x) __builtin_expect(!!(x), 0)

#define NOINLINE   __attribute__((noinline))
#define ALWAYSINLINE inline __attribute__((always_inline))

// ========== 平台相关宏 ==========

#ifdef __linux__
    #define MY_JVM_OS_LINUX 1
#elif defined(_WIN32)
    #define MY_JVM_OS_WINDOWS 1
#elif defined(__APPLE__)
    #define MY_JVM_OS_MACOS 1
#endif

#ifdef __x86_64__
    #define MY_JVM_CPU_X86 1
    #define MY_JVM64 1
#elif defined(__i386__)
    #define MY_JVM_CPU_X86 1
    #define MY_JVM32 1
#elif defined(__aarch64__)
    #define MY_JVM_CPU_AARCH64 1
    #define MY_JVM64 1
#endif

// ========== 基本类型宏 ==========

#ifdef MY_JVM64
    #define SIZE_T_FORMAT "%lu"
    #define UINT_FORMAT "%u"
#else
    #define SIZE_T_FORMAT "%u"
    #define UINT_FORMAT "%u"
#endif

// ========== 调试相关宏 ==========

#define MY_JVM_INCREMENT_COUNT(x) ((x)++)

#endif // MY_JVM_UTILITIES_MACROS_HPP
