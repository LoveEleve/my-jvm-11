#ifndef SHARE_UTILITIES_BYTES_HPP
#define SHARE_UTILITIES_BYTES_HPP

// ============================================================================
// Mini JVM - 字节序处理
// 对应 HotSpot: utilities/bytes.hpp + cpu/x86/bytes_x86.hpp
//              + os_cpu/linux_x86/bytes_linux_x86.inline.hpp
//
// HotSpot 的字节序处理分三层文件（跨平台设计），我们合并为一个文件。
//
// 核心问题：
//   Java .class 文件使用大端序 (Big-Endian)
//   x86 CPU 使用小端序 (Little-Endian)
//   所以读取 u2/u4/u8 时，必须翻转字节序。
//
// 例如 .class 文件中 u2 值 0x0034 (version 52):
//   文件字节: [0x00, 0x34]   ← 大端：高字节在前
//   x86 内存: [0x34, 0x00]   ← 小端：低字节在前
//   直接读 = 0x3400 (错！)
//   swap后 = 0x0034 (对！)
// ============================================================================

#include "utilities/globalDefinitions.hpp"
#include <byteswap.h>   // Linux: bswap_16, bswap_32, bswap_64
#include <cstring>       // memcpy

// ----------------------------------------------------------------------------
// Endian — 字节序描述
// HotSpot: bytes.hpp 中的 Endian 类
// ----------------------------------------------------------------------------

class Endian {
public:
    enum Order {
        LITTLE,
        BIG,
        JAVA   = BIG,      // Java 字节序 ≡ 大端序（JVM 规范规定）
        NATIVE = LITTLE     // x86 = 小端序
    };

    // x86 上始终返回 true（小端 != 大端）
    static inline bool is_Java_byte_ordering_different() {
        return NATIVE != JAVA;
    }
};

// ----------------------------------------------------------------------------
// Bytes — 字节读写工具
// HotSpot: cpu/x86/bytes_x86.hpp + os_cpu/linux_x86/bytes_linux_x86.inline.hpp
// ----------------------------------------------------------------------------

class Bytes {
public:
    // ========== 字节序翻转 ==========

    // HotSpot 在 Linux x86_64 上使用 glibc 的 bswap 系列
    static inline u2 swap_u2(u2 x) { return bswap_16(x); }
    static inline u4 swap_u4(u4 x) { return bswap_32(x); }
    static inline u8 swap_u8(u8 x) { return bswap_64(x); }

    // ========== 本机字节序读取 ==========

    // HotSpot 的模板方法：对齐时直接解引用，未对齐时用 memcpy
    template <typename T>
    static inline T get_native(const void* p) {
        T x;
        memcpy(&x, p, sizeof(T));  // memcpy 安全处理未对齐
        return x;
    }

    // ========== Java (大端序) 读取 ==========
    // 这是 ClassFileStream 最终调用的方法

    static inline u2 get_Java_u2(const address p) {
        u2 x = get_native<u2>(p);
        if (Endian::is_Java_byte_ordering_different()) {
            x = swap_u2(x);
        }
        return x;
    }

    static inline u4 get_Java_u4(const address p) {
        u4 x = get_native<u4>(p);
        if (Endian::is_Java_byte_ordering_different()) {
            x = swap_u4(x);
        }
        return x;
    }

    static inline u8 get_Java_u8(const address p) {
        u8 x = get_native<u8>(p);
        if (Endian::is_Java_byte_ordering_different()) {
            x = swap_u8(x);
        }
        return x;
    }

    // ========== Java (大端序) 写入 ==========
    // 将主机序值写为大端序字节

    static inline void put_Java_u2(address p, u2 x) {
        if (Endian::is_Java_byte_ordering_different()) {
            x = swap_u2(x);
        }
        memcpy(p, &x, sizeof(u2));
    }

    static inline void put_Java_u4(address p, u4 x) {
        if (Endian::is_Java_byte_ordering_different()) {
            x = swap_u4(x);
        }
        memcpy(p, &x, sizeof(u4));
    }
};

#endif // SHARE_UTILITIES_BYTES_HPP
