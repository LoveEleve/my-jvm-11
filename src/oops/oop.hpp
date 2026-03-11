/*
 * my_jvm - OOP (Ordinary Object Pointer) definitions
 * 
 * Java 对象在 JVM 内部的表示，来自 OpenJDK 11 hotspot/src/share/vm/oops/oop.hpp
 * 简化版本
 * 
 * 注意：oopDesc 是 Java 堆上的对象，Klass 是 Metaspace 里的元数据，两者独立
 */

#ifndef MY_JVM_OOPS_OOP_HPP
#define MY_JVM_OOPS_OOP_HPP

#include "globalDefinitions.hpp"
#include "markOop.hpp"
#include "klass.hpp"

// ========== oopDesc 类 ==========
// 这是所有 Java 对象在 JVM 内部的基类
// 存放在 Java 堆上

class oopDesc {
private:
    // 对象头：mark word
    // 存储对象的哈希码、GC 信息、锁状态等
    // volatile 是必须的，因为多个线程会并发修改 mark word
    volatile markOop _mark;
    
    // 元数据指针
    // 64位模式下可能是压缩的 Klass 指针
    union _metadata {
        Klass*      _klass;        // 64位模式：直接指针
        narrowKlass _compressed_klass;  // 64位模式：压缩指针
    } _metadata;

public:
    // ========== 构造函数 ==========
    
    oopDesc() : _mark(0), _metadata{._klass = nullptr} {}
    
    // ========== mark word 访问 ==========
    
    markOop mark() const { return _mark; }
    markOop mark_raw() const { return _mark; }
    markOop* mark_addr_raw() const { return (markOop*)&_mark; }
    
    void set_mark(volatile markOop m) { _mark = m; }
    void set_mark_raw(volatile markOop m) { _mark = m; }
    
    // ========== Klass 指针访问 ==========
    
    Klass* klass() const { return _metadata._klass; }
    void set_klass(Klass* k) { _metadata._klass = k; }
    
    // 压缩指针版本
    narrowKlass compressed_klass() const { return _metadata._compressed_klass; }
    void set_compressed_klass(narrowKlass nk) { _metadata._compressed_klass = nk; }
    
    // ========== 对象类型判断 ==========
    // 注意：需要完整类型，所以这些函数在这里简化实现
    
    // 判断是否为数组
    bool is_array() const { 
        return _metadata._klass != nullptr && _metadata._klass->is_array_klass();
    }
    
    // 判断是否为对象数组
    bool is_objArray() const { 
        return _metadata._klass != nullptr && _metadata._klass->is_objArray_klass();
    }
    
    // 判断是否为基本类型数组
    bool is_typeArray() const { 
        return _metadata._klass != nullptr && _metadata._klass->is_typeArray_klass();
    }
    
    // 判断是否为实例对象
    bool is_instance() const { 
        return _metadata._klass != nullptr && _metadata._klass->is_instance_klass();
    }
    
    // ========== 锁状态判断 ==========
    
    bool is_unlocked() const { return markWord_is_unlocked(_mark); }
    bool is_locked() const { return markWord_is_lightweight_locked(_mark); }
    bool is_heavyweight_locked() const { return markWord_is_heavyweight_locked(_mark); }
    bool is_gc_marked() const { return markWord_is_gc_marked(_mark); }
};


// ========== oop 类型别名 ==========

// 普通对象指针
typedef oopDesc* oop;


// ========== 数组 oop ==========

class arrayOopDesc : public oopDesc {
public:
    // 数组长度（负数长度表示空数组）
    int32_t length() const;
    void set_length(int32_t len);
    
    // 获取数组元数据
    // Klass* klass() const;  // 继承自 oopDesc
};

// 数组对象指针
typedef arrayOopDesc* arrayOop;


#endif // MY_JVM_OOPS_OOP_HPP
