#ifndef SHARE_OOPS_ARRAYOOP_HPP
#define SHARE_OOPS_ARRAYOOP_HPP

// ============================================================================
// Mini JVM - arrayOopDesc（数组对象基类）
// 对应 HotSpot: src/hotspot/share/oops/arrayOop.hpp
//
// 所有 Java 数组（int[], Object[], byte[] 等）的基类。
// 在 oopDesc 头之后紧跟一个 _length 字段。
//
// 内存布局（LP64 非压缩指针）：
//   ┌─────────────────────────┐  offset 0
//   │ _mark    (markOop, 8B)  │  来自 oopDesc
//   ├─────────────────────────┤  offset 8
//   │ _klass   (Klass*, 8B)   │  来自 oopDesc
//   ├─────────────────────────┤  offset 16
//   │ _length  (jint, 4B)     │  数组长度
//   ├─────────────────────────┤  offset 20
//   │ [pad]    (4B)           │  对齐到 8 字节
//   ├─────────────────────────┤  offset 24
//   │ elements...             │  数组元素数据
//   └─────────────────────────┘
//
// HotSpot 中 header_size = 3 HeapWords (24 bytes) in LP64 non-compressed
// 数据区从 offset 24 (= 3 * HeapWordSize) 开始
// ============================================================================

#include "oops/oop.hpp"

class arrayOopDesc : public oopDesc {
private:
    // 数组长度（紧跟在 oopDesc 头部之后）
    // HotSpot: 实际上是 *(int*)((address)this + length_offset_in_bytes())
    // 我们用显式字段
    jint _length;

    // 填充到 8 字节对齐（LP64 下 oopDesc=16B + length=4B = 20B → 需要 4B padding）
    jint _padding;

public:
    // 数组长度
    jint length() const { return _length; }
    void set_length(jint len) { _length = len; }

    // length 字段在对象中的偏移
    static int length_offset_in_bytes() {
        // oopDesc header (16B) 之后就是 _length
        return sizeof(oopDesc);  // = 16
    }

    // 数组头大小（以 HeapWord 为单位）
    // LP64 非压缩：sizeof(arrayOopDesc) = 24 → 24/8 = 3
    static int header_size_in_bytes() {
        return sizeof(arrayOopDesc);  // = 24
    }

    static int header_size() {
        return sizeof(arrayOopDesc) / HeapWordSize;  // = 3
    }

    // 数组数据区起始偏移（字节）
    // 元素紧跟在头部之后
    static int base_offset_in_bytes() {
        return sizeof(arrayOopDesc);  // = 24
    }

    // 数据区起始地址
    void* base() const {
        return (void*)((address)this + base_offset_in_bytes());
    }
};

#endif // SHARE_OOPS_ARRAYOOP_HPP
