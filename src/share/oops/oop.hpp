#ifndef SHARE_OOPS_OOP_HPP
#define SHARE_OOPS_OOP_HPP

// ============================================================================
// Mini JVM - oopDesc（Java 对象在 JVM 中的 C++ 表示）
// 对应 HotSpot: src/hotspot/share/oops/oop.hpp
//
// oopDesc 是所有 Java 对象在 JVM 内部的基类。
// 每个 Java 对象在堆中的内存布局：
//
//   ┌─────────────────────┐
//   │  _mark (markOop)    │  8 bytes — mark word（锁状态、hash、GC年龄）
//   ├─────────────────────┤
//   │  _metadata._klass   │  8 bytes — 指向 Klass 的指针（或 4 bytes 压缩）
//   ├─────────────────────┤
//   │  instance fields     │  ... — 实例字段数据
//   └─────────────────────┘
//
// 关键设计：
//   1. oopDesc 没有虚函数 → 对象头不包含 vptr → 省空间
//   2. 所有"虚"行为通过 _metadata._klass 转发到 Klass 的虚函数
//   3. _mark 用 volatile 修饰，因为锁操作需要 CAS
//   4. _metadata 是 union：非压缩时用 Klass*，压缩时用 narrowKlass (u4)
//
// LP64 非压缩指针下：header_size = sizeof(oopDesc) = 16 bytes = 2 HeapWords
// ============================================================================

#include "oops/markOop.hpp"

// 前向声明
class Klass;

class oopDesc {
private:
    volatile markOop _mark;       // Mark Word（8 bytes）

    // Klass 指针联合体
    // 非压缩：直接用 Klass*（8 bytes）
    // 压缩：用 narrowKlass（4 bytes）+ 4 bytes gap
    union _metadata {
        Klass*      _klass;
        juint       _compressed_klass;  // narrowKlass
    } _metadata;

public:
    // ======== Mark Word 访问 ========

    markOop mark()     const { return _mark; }
    markOop mark_raw() const { return _mark; }

    void set_mark(volatile markOop m)     { _mark = m; }
    void set_mark_raw(volatile markOop m) { _mark = m; }

    // 初始化 mark word 为 prototype（无锁、无 hash、age=0）
    void init_mark() { set_mark(markOopDesc::prototype()); }

    // ======== Klass 指针访问 ========

    Klass* klass() const { return _metadata._klass; }

    void set_klass(Klass* k) { _metadata._klass = k; }

    Klass* klass_or_null() const { return _metadata._klass; }

    // ======== 头部大小 ========

    // 对象头大小（以 HeapWord 为单位）
    // LP64 非压缩：sizeof(oopDesc) = 16, HeapWordSize = 8 → header_size = 2
    static int header_size() { return sizeof(oopDesc) / HeapWordSize; }

    // ======== 字段访问（通过字节偏移）========
    // HotSpot: oop.hpp 约 131-195 行

    void* field_addr(int offset) const {
        return (void*)((address)this + offset);
    }

    // 各种类型的字段读写
    jbyte    byte_field(int offset) const    { return *(jbyte*)field_addr(offset); }
    void     byte_field_put(int offset, jbyte v)    { *(jbyte*)field_addr(offset) = v; }

    jchar    char_field(int offset) const    { return *(jchar*)field_addr(offset); }
    void     char_field_put(int offset, jchar v)    { *(jchar*)field_addr(offset) = v; }

    jboolean bool_field(int offset) const    { return *(jboolean*)field_addr(offset); }
    void     bool_field_put(int offset, jboolean v) { *(jboolean*)field_addr(offset) = v; }

    jshort   short_field(int offset) const   { return *(jshort*)field_addr(offset); }
    void     short_field_put(int offset, jshort v)  { *(jshort*)field_addr(offset) = v; }

    jint     int_field(int offset) const     { return *(jint*)field_addr(offset); }
    void     int_field_put(int offset, jint v)      { *(jint*)field_addr(offset) = v; }

    jlong    long_field(int offset) const    { return *(jlong*)field_addr(offset); }
    void     long_field_put(int offset, jlong v)    { *(jlong*)field_addr(offset) = v; }

    jfloat   float_field(int offset) const   { return *(jfloat*)field_addr(offset); }
    void     float_field_put(int offset, jfloat v)  { *(jfloat*)field_addr(offset) = v; }

    jdouble  double_field(int offset) const  { return *(jdouble*)field_addr(offset); }
    void     double_field_put(int offset, jdouble v){ *(jdouble*)field_addr(offset) = v; }

    // oop 字段（引用类型字段）
    oopDesc* obj_field(int offset) const     { return *(oopDesc**)field_addr(offset); }
    void     obj_field_put(int offset, oopDesc* v)  { *(oopDesc**)field_addr(offset) = v; }

    // ======== Lock 状态查询（转发到 markOop）========

    bool is_locked()   const { return mark()->is_locked(); }
    bool is_unlocked() const { return mark()->is_unlocked(); }

    // ======== GC 相关 ========

    bool is_gc_marked() const { return mark()->is_marked(); }

    // GC 年龄
    juint age() const { return mark()->age(); }
    void  incr_age()  { set_mark(mark()->incr_age()); }

    // ======== 代码生成用的偏移量常量 ========

    static int mark_offset_in_bytes()  { return (int)(intptr_t)&((oopDesc*)nullptr)->_mark; }
    static int klass_offset_in_bytes() { return (int)(intptr_t)&((oopDesc*)nullptr)->_metadata._klass; }

    // ======== 调试打印 ========

    void print_on(FILE* out) const {
        fprintf(out, "oop(%p) mark=", (void*)this);
        if (_mark != nullptr) {
            _mark->print_on(out);
        } else {
            fprintf(out, "null");
        }
        fprintf(out, " klass=%p", (void*)_metadata._klass);
    }
};

#endif // SHARE_OOPS_OOP_HPP
