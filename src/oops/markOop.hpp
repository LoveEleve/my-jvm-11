/*
 * my_jvm - Mark Word encoding
 * 
 * 参考 OpenJDK 11 hotspot/src/hotspot/share/oops/markOop.hpp
 * 简化版本
 * 
 * 重要：markOop 是 markOopDesc*（指针类型），不是整数！
 */

#ifndef MY_JVM_OOPS_MARKOOP_HPP
#define MY_JVM_OOPS_MARKOOP_HPP

#include "globalDefinitions.hpp"

// ========== markOopDesc 类 ==========
// 参考：markOop.hpp 第 104 行
// 为了简化，不继承 oopDesc

class markOopDesc {
private:
    // 获取值（将 this 指针转换为 uintptr_t）
    uintptr_t value() const { return (uintptr_t)this; }

public:
    // ========== 锁状态编码 ==========
    // 参考：markOop.hpp 第 150-155 行
    
    enum {
        locked_value       = 0,    // 轻量级锁
        unlocked_value     = 1,    // 无锁
        monitor_value      = 2,    // 重量级锁
        marked_value       = 3,    // GC 标记
        biased_lock_pattern = 5    // 偏向锁
    };

    // ========== 锁状态判断 ==========
    
    bool is_locked() const {
        return (value() & 0x3) != unlocked_value;
    }
    
    bool is_unlocked() const {
        return (value() & 0x7) == unlocked_value;
    }
    
    bool is_marked() const {
        return (value() & 0x3) == marked_value;
    }
    
    bool is_being_inflated() const {
        return value() == 0;
    }

    // ========== 偏向锁 ==========
    
    bool has_bias_pattern() const {
        return (value() & 0x7) == biased_lock_pattern;
    }

    // ========== 哈希码 ==========
    
    intptr_t hash() const {
        return (intptr_t)((value() >> 8) & 0x7FFFFFFF);
    }

    // ========== 对象年龄 ==========
    
    uint age() const {
        return (uint)((value() >> 3) & 0xF);  // age_shift = 3
    }
    
    // 获取原始值
    uintptr_t raw_value() const { return value(); }
};

// ========== markOop 类型别名 ==========
// markOop 是指向 markOopDesc 的指针

typedef markOopDesc* markOop;
typedef markOop      markWord;

// ========== 辅助函数 ==========

inline markOop markWord_unlocked() {
    return (markOop)(uintptr_t)markOopDesc::unlocked_value;
}

inline markOop markWord_lightweight_locked(void* lock_rec) {
    return (markOop)((uintptr_t)lock_rec);
}

inline markOop markWord_heavyweight_locked(void* monitor) {
    return (markOop)((uintptr_t)monitor | 0x2);
}

inline markOop markWord_gc_marked() {
    return (markOop)(uintptr_t)markOopDesc::marked_value;
}

// 偏向锁
// 格式：[JavaThread*:54 | epoch:2 | unused:1 | age:4 | biased_lock:1 | lock:2]
// JavaThread 指针是 256 字节对齐的，低8位天然为0，所以直接 OR 进去即可，不需要左移
// age_shift = 3, epoch_shift = 8
inline markOop markWord_biased(void* thread, int age, int epoch) {
    uintptr_t ptr = (uintptr_t)thread;  // 直接用，不需要左移！
    ptr |= ((uintptr_t)epoch << 8) |     // epoch_shift = hash_shift = 8
           ((uintptr_t)age << 3) |       // age_shift = 3
           0x5;                           // biased_lock_pattern = 5
    return (markOop)ptr;
}

// 设置哈希码
// hash_shift = 8, hash_mask = 0x7FFFFFFF
inline markOop markWord_with_hash(markOop m, uintptr_t hash) {
    uintptr_t hash_mask_in_place = ((uintptr_t)0x7FFFFFFF) << 8;
    uintptr_t v = m->raw_value();
    v = (v & ~hash_mask_in_place) | ((hash & 0x7FFFFFFF) << 8);
    return (markOop)v;
}

// 提取哈希码
inline uintptr_t markWord_hash(markOop m) {
    return m->hash();
}

// 简化版判断函数
inline bool markWord_is_unlocked(markOop m) {
    return m->is_unlocked();
}

inline bool markWord_is_lightweight_locked(markOop m) {
    uintptr_t v = m->raw_value();
    return (v & 0x3) == 0;
}

inline bool markWord_is_heavyweight_locked(markOop m) {
    return (m->raw_value() & 0x3) == 0x2;
}

inline bool markWord_is_gc_marked(markOop m) {
    return m->is_marked();
}

#endif // MY_JVM_OOPS_MARKOOP_HPP
