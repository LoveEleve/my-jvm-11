#ifndef SHARE_OOPS_INSTANCEOOP_HPP
#define SHARE_OOPS_INSTANCEOOP_HPP

// ============================================================================
// Mini JVM - instanceOopDesc
// 对应 HotSpot: src/hotspot/share/oops/instanceOop.hpp
//
// instanceOopDesc 表示一个普通的 Java 实例对象（非数组）。
// 例如 new HashMap() 创建的就是 instanceOop。
//
// 内存布局：
//   ┌─────────────────────────┐
//   │ oopDesc header (16 B)   │  mark + klass
//   ├─────────────────────────┤
//   │ instance fields          │  按 FieldAllocationStrategy 排列
//   └─────────────────────────┘
//
// 几乎没有额外逻辑，只是定义了 header_size() 和 base_offset_in_bytes()。
// HotSpot 中 instanceOopDesc.hpp 也只有寥寥几十行。
// ============================================================================

#include "oops/oop.hpp"

class instanceOopDesc : public oopDesc {
public:
    // 对象头大小（以 HeapWord 为单位）
    // LP64 非压缩：sizeof(instanceOopDesc) == sizeof(oopDesc) == 16
    //  → header_size = 16 / 8 = 2
    static int header_size() { return sizeof(instanceOopDesc) / HeapWordSize; }

    // 第一个实例字段的字节偏移
    // LP64 非压缩指针：字段紧跟在 16 字节头后面
    // 如果启用压缩类指针，可能从 12 字节处开始（利用 klass gap）
    static int base_offset_in_bytes() {
        return sizeof(instanceOopDesc);  // = 16 (LP64 no compressed)
    }
};

#endif // SHARE_OOPS_INSTANCEOOP_HPP
