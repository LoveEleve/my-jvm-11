/*
 * my_jvm - Basic type definitions
 * 
 * 参考 OpenJDK 11 hotspot/src/hotspot/share/utilities/globalDefinitions.hpp
 * 和 hotspot/src/hotspot/share/oops/oopsHierarchy.hpp
 * 
 * 简化版本，只保留最核心的类型
 */

#ifndef MY_JVM_UTILITIES_GLOBALDEFINITIONS_HPP
#define MY_JVM_UTILITIES_GLOBALDEFINITIONS_HPP

#include <cstdint>
#include <cstddef>
#include <climits>
#include <cfloat>
#include "macros.hpp"

// ========== Java 类型定义 ==========
// 参考：src/java.base/unix/native/include/jni_md.h

typedef bool               jboolean;    // true/false
typedef int8_t           jbyte;        // 8位有符号
typedef int16_t          jshort;       // 16位有符号
typedef int32_t          jint;         // 32位有符号（来自 jni_md.h）
typedef int64_t          jlong;        // 64位有符号
typedef float             jfloat;       // 32位浮点
typedef double            jdouble;      // 64位浮点
typedef uint16_t         jchar;        // 16位无符号（Java char 是 UTF-16）

// 有符号和无符号别名
typedef uint8_t          juint8_t;
typedef uint16_t         juint16_t;
typedef uint32_t         juint;         // 32位无符号
typedef uint64_t         julong;        // 64位无符号

// ========== JVM 内部类型 ==========
// 参考：globalDefinitions.hpp 第 452-465 行

typedef uint8_t          u1;   // 1字节无符号（class 文件用）
typedef uint16_t         u2;   // 2字节无符号
typedef uint32_t         u4;   // 4字节无符号
typedef uint64_t         u8;   // 8字节无符号

typedef int8_t           s1;   // 1字节有符号
typedef int16_t          s2;   // 2字节有符号
typedef int32_t          s4;   // 4字节有符号
typedef int64_t          s8;   // 8字节有符号

// ========== 指针类型 ==========
// 标准库已定义，直接使用

typedef uintptr_t address;
typedef uintptr_t HeapWord;

// ========== OOP 相关类型 - 参考 oopsHierarchy.hpp ==========

// oopDesc 前向声明（Java 堆上的对象）
class oopDesc;

// oop: ordinary object pointer（指向 Java 堆上的对象）
typedef class oopDesc* oop;

// Klass 前向声明（Metaspace 里的类元数据）
class Klass;
typedef class Klass*    KlassPtr;

// narrowKlass: 压缩的类指针（32位）
typedef uint32_t narrowKlass;

// Method 前向声明（Metaspace 里的方法元数据）
class Method;
typedef class Method*    MethodPtr;

// markOopDesc 前向声明
class markOopDesc;

// markOop: 指向对象头的指针（markOopDesc*）
// 注意：这是指针类型，不是整数！
typedef class markOopDesc* markOop;

// markWord: markOop 的别名（历史原因）
typedef markOop markWord;

// ========== 常量定义 ==========

#ifdef MY_JVM64
    #define BitsPerWord         64
    #define BytesPerWord        8
    #define WordsPerDouble      2
#else
    #define BitsPerWord         32
    #define BytesPerWord        4
    #define WordsPerDouble      2
#endif

#define BitsPerByte        8
#define BitsPerInt         32
#define BitsPerLong        64

// ========== 对齐函数 ==========

#define align_mask(alignment) ((alignment) - 1)
#define align_down_(size, alignment) ((size) & ~align_mask(alignment))
#define align_up_(size, alignment) (align_down_((size) + align_mask(alignment), (alignment)))
#define is_aligned_(size, alignment) (((size) & align_mask(alignment)) == 0)

template <typename T, typename A>
inline T align_size_up(T size, A alignment) {
  return (T)align_up_((size_t)size, (size_t)alignment);
}

template <typename T, typename A>
inline T align_size_down(T size, A alignment) {
  return (T)align_down_((size_t)size, (size_t)alignment);
}

template <typename T, typename A>
inline T align_up(T size, A alignment) {
  return align_size_up(size, alignment);
}

template <typename T, typename A>
inline T align_down(T size, A alignment) {
  return align_size_down(size, alignment);
}

template <typename T, typename A>
inline bool is_aligned(T size, A alignment) {
  return is_aligned_((size_t)size, (size_t)alignment);
}

// ========== 格式化输出 ==========

#define BOOL_TO_STR(b) ((b) ? "true" : "false")

// ========== Copy 类前向声明 ==========

class Copy;

// ========== NOT_NOINT / NOINLINE ==========

#ifndef NOT_NOINLINE
#define NOT_NOINLINE
#endif

// ========== 格式化宏 ==========

#ifndef INT64_FORMAT
#define INT64_FORMAT "%" "ld"
#endif

#ifndef UINT64_FORMAT
#define UINT64_FORMAT "%" "lu"
#endif

#ifndef INT32_FORMAT
#define INT32_FORMAT "%" "d"
#endif

#ifndef UINT32_FORMAT
#define UINT32_FORMAT "%" "u"
#endif

#ifndef PTR_FORMAT
#define PTR_FORMAT "%" "p"
#endif

#ifndef SIZE_FORMAT
#define SIZE_FORMAT "%" "zu"
#endif

// ========== inline 函数 ==========

inline bool is_power_of_2(size_t x) {
  return x != 0 && (x & (x - 1)) == 0;
}

#endif // MY_JVM_UTILITIES_GLOBALDEFINITIONS_HPP
