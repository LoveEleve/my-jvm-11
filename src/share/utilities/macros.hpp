#ifndef SHARE_UTILITIES_MACROS_HPP
#define SHARE_UTILITIES_MACROS_HPP

// ============================================================================
// Mini JVM - 常用宏
// 对应 HotSpot: src/hotspot/share/utilities/macros.hpp
// ============================================================================

// 字符串化
#define STR(x)  #x
#define XSTR(x) STR(x)

// 拼接 token
#define PASTE_TOKENS(x, y) x ## y

// 平台判断
#ifdef LINUX
#define NOT_LINUX(code)
#define LINUX_ONLY(code)  code
#else
#define NOT_LINUX(code)   code
#define LINUX_ONLY(code)
#endif

// 64 位判断
#ifdef _LP64
#define LP64_ONLY(code)     code
#define NOT_LP64(code)
#else
#define LP64_ONLY(code)
#define NOT_LP64(code)      code
#endif

// Debug / Product 构建
#ifdef ASSERT
#define DEBUG_ONLY(code)    code
#define NOT_DEBUG(code)
#else
#define DEBUG_ONLY(code)
#define NOT_DEBUG(code)     code
#endif

// GC 相关（我们只支持 G1）
#define INCLUDE_G1GC 1

#if INCLUDE_G1GC
#define G1GC_ONLY(code)     code
#define NOT_G1GC(code)
#else
#define G1GC_ONLY(code)
#define NOT_G1GC(code)      code
#endif

#endif // SHARE_UTILITIES_MACROS_HPP
