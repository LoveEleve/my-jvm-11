#ifndef SHARE_UTILITIES_GLOBALDEFINITIONS_HPP
#define SHARE_UTILITIES_GLOBALDEFINITIONS_HPP

// ============================================================================
// Mini JVM - 基础类型定义
// 对应 HotSpot: src/hotspot/share/utilities/globalDefinitions.hpp
//
// HotSpot 原版通过 jni_md.h → jni.h → globalDefinitions.hpp 层层引入类型。
// 我们精简为一个文件，保留核心定义。
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cassert>

// ----------------------------------------------------------------------------
// 1. JNI 基础类型
//    HotSpot 中来自 jni_md.h (平台相关) + jni.h
//    在 LP64 (Linux x86_64) 下：
// ----------------------------------------------------------------------------

typedef signed char    jbyte;      // 8-bit  有符号
typedef unsigned char  jboolean;   // 8-bit  无符号 (JNI_TRUE=1, JNI_FALSE=0)
typedef unsigned short jchar;      // 16-bit 无符号 (UTF-16)
typedef short          jshort;     // 16-bit 有符号
typedef int            jint;       // 32-bit 有符号
typedef long           jlong;      // 64-bit 有符号 (LP64下 long=64bit)
typedef float          jfloat;     // 32-bit 浮点
typedef double         jdouble;    // 64-bit 浮点

// JNI 无符号变体（HotSpot 扩展）
typedef unsigned char  jubyte;
typedef unsigned short jushort;
typedef unsigned int   juint;
typedef unsigned long  julong;

// JNI 常量
#define JNI_FALSE 0
#define JNI_TRUE  1

// ----------------------------------------------------------------------------
// 2. VM 扩展类型
//    HotSpot: globalDefinitions.hpp 约 349-372 行
// ----------------------------------------------------------------------------

typedef intptr_t   intx;           // 平台字长有符号整数 (64-bit on LP64)
typedef uintptr_t  uintx;         // 平台字长无符号整数

typedef unsigned char  u_char;
typedef u_char*        address;    // 通用字节地址（最核心的指针类型）
typedef uintptr_t      address_word; // 可存放指针的无符号整数

// ----------------------------------------------------------------------------
// 3. Class 文件格式类型
//    HotSpot: globalDefinitions.hpp 约 452-465 行
//    JVM 规范定义的 u1/u2/u4/u8 类型，用于解析 .class 文件
// ----------------------------------------------------------------------------

typedef jubyte  u1;    // unsigned 1 字节
typedef jushort u2;    // unsigned 2 字节
typedef juint   u4;    // unsigned 4 字节
typedef julong  u8;    // unsigned 8 字节

typedef jbyte   s1;    // signed 1 字节
typedef jshort  s2;    // signed 2 字节
typedef jint    s4;    // signed 4 字节
typedef jlong   s8;    // signed 8 字节

// ----------------------------------------------------------------------------
// 4. 字长相关常量
//    HotSpot: globalDefinitions.hpp 约 144-258 行
// ----------------------------------------------------------------------------

// LP64 下的值
const int LogBytesPerShort  = 1;
const int LogBytesPerInt    = 2;
const int LogBytesPerWord   = 3;   // log2(8) = 3
const int LogBytesPerLong   = 3;

const int BytesPerShort = 1 << LogBytesPerShort;  // 2
const int BytesPerInt   = 1 << LogBytesPerInt;     // 4
const int BytesPerWord  = 1 << LogBytesPerWord;    // 8
const int BytesPerLong  = 1 << LogBytesPerLong;    // 8

const int LogBitsPerByte = 3;
const int LogBitsPerWord = LogBitsPerByte + LogBytesPerWord;  // 3+3 = 6

const int BitsPerByte = 1 << LogBitsPerByte;   // 8
const int BitsPerWord = 1 << LogBitsPerWord;    // 64
const int BitsPerLong = BytesPerLong * BitsPerByte;  // 64

const int oopSize  = sizeof(char*);   // oop 指针大小 = 8 (LP64)
const int wordSize = sizeof(char*);   // 字大小 = 8

// 大小常量
const size_t K = 1024;
const size_t M = K * K;
const size_t G = M * K;

// jlong 边界
const jlong min_jlong = (jlong)0x8000000000000000LL;
const jlong max_jlong = (jlong)0x7fffffffffffffffLL;
const jint  min_jint  = (jint)0x80000000;
const jint  max_jint  = (jint)0x7fffffff;

// ----------------------------------------------------------------------------
// 5. HeapWord / MetaWord
//    HotSpot: globalDefinitions.hpp 约 212-243 行
//    不透明类型，用于堆/元空间的指针运算单位
// ----------------------------------------------------------------------------

class HeapWord {
    friend class VMStructs;
private:
    char* i;   // "不透明"——不让你直接访问内容，强制通过指针运算
public:
    // 允许指针算术
};

class MetaWord {
private:
    char* i;
};

const int HeapWordSize        = sizeof(HeapWord);   // = 8 (LP64)
const int LogHeapWordSize     = LogBytesPerWord;     // = 3
const int HeapWordsPerLong    = BytesPerLong / HeapWordSize;  // = 1
const int LogHeapWordsPerLong = LogBytesPerLong - LogHeapWordSize; // = 0

// ----------------------------------------------------------------------------
// 6. BasicType 枚举
//    HotSpot: globalDefinitions.hpp 约 626-644 行
//    Java 类型在 JVM 内部的编码
// ----------------------------------------------------------------------------

enum BasicType {
    T_BOOLEAN  =  4,
    T_CHAR     =  5,
    T_FLOAT    =  6,
    T_DOUBLE   =  7,
    T_BYTE     =  8,
    T_SHORT    =  9,
    T_INT      = 10,
    T_LONG     = 11,
    T_OBJECT   = 12,
    T_ARRAY    = 13,
    T_VOID     = 14,
    T_ADDRESS  = 15,
    T_NARROWOOP   = 16,
    T_METADATA    = 17,
    T_NARROWKLASS = 18,
    T_CONFLICT = 19,
    T_ILLEGAL  = 99
};

// BasicType → 大小映射（字节数）
inline int type2size_in_bytes(BasicType t) {
    switch (t) {
        case T_BOOLEAN: return 1;
        case T_BYTE:    return 1;
        case T_CHAR:    return 2;
        case T_SHORT:   return 2;
        case T_INT:     return 4;
        case T_FLOAT:   return 4;
        case T_LONG:    return 8;
        case T_DOUBLE:  return 8;
        case T_OBJECT:  return oopSize;
        case T_ARRAY:   return oopSize;
        case T_ADDRESS: return sizeof(char*);
        default:        return -1;
    }
}

// ----------------------------------------------------------------------------
// 7. JavaThreadState 枚举
//    HotSpot: globalDefinitions.hpp 约 890-903 行
// ----------------------------------------------------------------------------

enum JavaThreadState {
    _thread_uninitialized = 0,  // 未初始化
    _thread_new           = 2,  // 刚创建
    _thread_new_trans     = 3,  // new → 其他的过渡
    _thread_in_native     = 4,  // 执行 native 代码
    _thread_in_native_trans = 5,
    _thread_in_vm         = 6,  // 执行 VM 内部代码
    _thread_in_vm_trans   = 7,
    _thread_in_Java       = 8,  // 执行 Java 字节码
    _thread_in_Java_trans = 9,
    _thread_blocked       = 10, // 阻塞（等锁等）
    _thread_blocked_trans = 11
};

// ----------------------------------------------------------------------------
// 8. 工具宏
// ----------------------------------------------------------------------------

// 对齐
inline bool is_power_of_2(intx x)  { return x > 0 && (x & (x - 1)) == 0; }
inline bool is_power_of_2(uintx x) { return x > 0 && (x & (x - 1)) == 0; }

// 向上对齐到 alignment 的倍数
inline uintx align_up(uintx size, uintx alignment) {
    assert(is_power_of_2(alignment));
    uintx mask = alignment - 1;
    return (size + mask) & ~mask;
}

inline void* align_up(void* addr, size_t alignment) {
    return (void*)align_up((uintx)addr, (uintx)alignment);
}

// 向下对齐
inline uintx align_down(uintx size, uintx alignment) {
    assert(is_power_of_2(alignment));
    uintx mask = alignment - 1;
    return size & ~mask;
}

// 检查是否对齐
inline bool is_aligned(uintx size, uintx alignment) {
    return (size & (alignment - 1)) == 0;
}

inline bool is_aligned(void* addr, size_t alignment) {
    return is_aligned((uintx)addr, (uintx)alignment);
}

// MIN / MAX
template<class T> inline T MAX2(T a, T b) { return (a > b) ? a : b; }
template<class T> inline T MIN2(T a, T b) { return (a < b) ? a : b; }

// 位操作
constexpr inline intx nth_bit(int n)       { return (n >= BitsPerWord) ? 0 : ((intx)1 << n); }
constexpr inline intx right_n_bits(int n)  { return nth_bit(n) - 1; }

// 数组大小
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

// 禁止拷贝
#define NONCOPYABLE(C)  \
    C(const C&) = delete; \
    C& operator=(const C&) = delete

// 指针转换
#define CAST_TO_FN_PTR(func_type, value)   (reinterpret_cast<func_type>(value))
#define CAST_FROM_FN_PTR(new_type, func_ptr) ((new_type)((address_word)(func_ptr)))

// NULL 检查
#define NULL_CHECK(p) if ((p) == nullptr) return

#endif // SHARE_UTILITIES_GLOBALDEFINITIONS_HPP
