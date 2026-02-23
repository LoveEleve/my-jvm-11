#ifndef SHARE_OOPS_MARKOOP_HPP
#define SHARE_OOPS_MARKOOP_HPP

// ============================================================================
// Mini JVM - 对象头 Mark Word
// 对应 HotSpot: src/hotspot/share/oops/markOop.hpp
//
// markOopDesc 是对象头的第一个字（64 位），编码了对象的运行时状态。
//
// 关键理解：markOop 不是真正的对象！
//   markOopDesc 虽然"继承"自 oopDesc，但它本身就是一个被当作位域使用的指针值。
//   (uintptr_t)this 就是 mark word 的值。从不 new markOopDesc。
//   markOop = markOopDesc* 只是为了利用 C++ 方法调用语法。
//
// 64 位 Mark Word 布局（标准对象，非偏向锁）：
//   ┌─────────────────────────────────────────────────────────────┐
//   │ unused:25 │ hash:31 │ unused:1 │ age:4 │ biased:1 │ lock:2│
//   └─────────────────────────────────────────────────────────────┘
//   MSB                                                       LSB
//
// Lock 状态编码（最低 2-3 位）：
//   [ptr             | 00]  轻量级锁（locked）  ptr 指向栈上的 displaced header
//   [header      | 0 | 01]  无锁（unlocked）    正常对象头
//   [ptr             | 10]  重量级锁（monitor）  ptr 指向 ObjectMonitor
//   [ptr             | 11]  GC 标记（marked）    仅 GC 时使用
//   [JavaThread* epoch age 1 | 01]  偏向锁
// ============================================================================

#include "utilities/globalDefinitions.hpp"

// 前向声明（避免循环依赖）
class oopDesc;

// markOopDesc 的精简实现
// HotSpot 中 markOopDesc : public oopDesc，我们为避免循环依赖独立定义
class markOopDesc {
private:
    // 不存储任何字段！this 指针本身就是 mark word 的值
    uintptr_t value() const { return (uintptr_t)this; }

public:
    // ====================================================================
    // 位域常量
    // HotSpot: markOop.hpp 约 111-165 行
    // ====================================================================

    // 各字段的位宽
    enum {
        age_bits          = 4,
        lock_bits         = 2,
        biased_lock_bits  = 1,
        max_hash_bits     = BitsPerWord - age_bits - lock_bits - biased_lock_bits,
        hash_bits         = max_hash_bits > 31 ? 31 : max_hash_bits,  // = 31
        cms_bits          = 1,   // LP64: CMS free block 标志位
        epoch_bits        = 2    // 偏向锁 epoch
    };

    // 各字段的移位量（从 LSB 开始）
    enum {
        lock_shift        = 0,
        biased_lock_shift = lock_bits,                      // = 2
        age_shift         = lock_bits + biased_lock_bits,   // = 3
        cms_shift         = age_shift + age_bits,           // = 7
        hash_shift        = cms_shift + cms_bits,           // = 8
        epoch_shift       = hash_shift                      // = 8 (偏向锁复用 hash 区域)
    };

    // 掩码
    enum {
        lock_mask              = right_n_bits(lock_bits),                  // 0x3
        lock_mask_in_place     = lock_mask << lock_shift,                 // 0x3
        biased_lock_mask       = right_n_bits(lock_bits + biased_lock_bits), // 0x7
        biased_lock_mask_in_place = biased_lock_mask << lock_shift,       // 0x7
        biased_lock_bit_in_place  = 1 << biased_lock_shift,              // 0x4
        age_mask               = right_n_bits(age_bits),                  // 0xF
        age_mask_in_place      = age_mask << age_shift,                   // 0x78
        epoch_mask             = right_n_bits(epoch_bits),                // 0x3
        epoch_mask_in_place    = epoch_mask << epoch_shift                // 0x300
    };

    // hash 掩码（太大放不进 enum，用 static const）
    static const uintptr_t hash_mask          = right_n_bits(hash_bits);
    static const uintptr_t hash_mask_in_place = hash_mask << hash_shift;

    // ====================================================================
    // Lock 状态值
    // ====================================================================

    enum {
        locked_value        = 0,   // 00 — 轻量级锁
        unlocked_value      = 1,   // 01 — 无锁
        monitor_value       = 2,   // 10 — 重量级锁
        marked_value        = 3,   // 11 — GC 标记
        biased_lock_pattern = 5    // 101 — 偏向锁
    };

    enum { no_hash = 0 };
    enum { no_hash_in_place = (address_word)no_hash << hash_shift };
    enum { no_lock_in_place = unlocked_value };
    enum { max_age = age_mask };

    // ====================================================================
    // 辅助函数
    // ====================================================================

    // 位操作辅助
    static uintptr_t mask_bits(uintptr_t x, uintptr_t m) { return x & m; }

    // ====================================================================
    // Lock 状态查询
    // HotSpot: markOop.hpp 约 206-215 行
    // ====================================================================

    bool is_locked()   const { return (mask_bits(value(), lock_mask_in_place) != unlocked_value); }
    bool is_unlocked() const { return (mask_bits(value(), biased_lock_mask_in_place) == unlocked_value); }
    bool is_marked()   const { return (mask_bits(value(), lock_mask_in_place) == marked_value); }
    bool is_neutral()  const { return (mask_bits(value(), biased_lock_mask_in_place) == unlocked_value); }

    bool has_bias_pattern() const {
        return (mask_bits(value(), biased_lock_mask_in_place) == biased_lock_pattern);
    }

    bool has_locker() const {
        return ((value() & lock_mask_in_place) == locked_value);
    }

    bool has_monitor() const {
        return ((value() & monitor_value) != 0);
    }

    // ====================================================================
    // Age（分代年龄，用于 GC 晋升）
    // ====================================================================

    juint age() const { return mask_bits(value() >> age_shift, age_mask); }

    markOopDesc* set_age(juint v) const {
        return (markOopDesc*)((value() & ~age_mask_in_place) |
                              (((uintptr_t)v & age_mask) << age_shift));
    }

    markOopDesc* incr_age() const {
        return age() == max_age ? (markOopDesc*)this : set_age(age() + 1);
    }

    // ====================================================================
    // Hash（identity hash code）
    // ====================================================================

    intptr_t hash() const {
        return mask_bits(value() >> hash_shift, hash_mask);
    }

    bool has_no_hash() const { return hash() == no_hash; }

    markOopDesc* copy_set_hash(intptr_t hash) const {
        intptr_t tmp = value() & (~hash_mask_in_place);
        tmp |= ((hash & hash_mask) << hash_shift);
        return (markOopDesc*)tmp;
    }

    // ====================================================================
    // GC 标记相关
    // ====================================================================

    markOopDesc* set_marked()   { return (markOopDesc*)((value() & ~lock_mask_in_place) | marked_value); }
    markOopDesc* set_unmarked() { return (markOopDesc*)((value() & ~lock_mask_in_place) | unlocked_value); }

    // ====================================================================
    // Prototype — 新对象的初始 mark word
    // HotSpot: markOop.hpp 约 345-347 行
    //
    // prototype() = no_hash | unlocked = 0x01
    // 即：hash=0, age=0, 无锁状态
    // ====================================================================

    static markOopDesc* prototype() {
        return (markOopDesc*)(intptr_t)(no_hash_in_place | no_lock_in_place);
    }

    // ====================================================================
    // 调试打印
    // ====================================================================

    void print_on(FILE* out) const {
        fprintf(out, "markOop(0x%016lx)", (unsigned long)value());
        if (is_neutral()) {
            fprintf(out, " [unlocked hash=%ld age=%u]",
                    (long)hash(), age());
        } else if (has_bias_pattern()) {
            fprintf(out, " [biased]");
        } else if (has_locker()) {
            fprintf(out, " [thin-locked]");
        } else if (has_monitor()) {
            fprintf(out, " [inflated]");
        } else if (is_marked()) {
            fprintf(out, " [gc-marked]");
        }
    }
};

// markOop 就是 markOopDesc 指针
typedef markOopDesc* markOop;

#endif // SHARE_OOPS_MARKOOP_HPP
