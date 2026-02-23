#ifndef SHARE_OOPS_KLASS_HPP
#define SHARE_OOPS_KLASS_HPP

// ============================================================================
// Mini JVM - Klass（类元数据基类）
// 对应 HotSpot: src/hotspot/share/oops/klass.hpp
//
// Klass 是所有类元数据的基类。每个 Java 类/接口/数组在 JVM 中都有一个
// 对应的 Klass 对象（位于 Metaspace）。
//
// OOP/Klass 二元结构的核心思想：
//   - oopDesc（堆中对象）没有 C++ 虚表 → 对象头只有 16 字节
//   - Klass（元空间）有 C++ 虚表 → 所有虚行为通过 klass 转发
//
// Klass 的核心职责：
//   1. _layout_helper: 告诉 GC 对象有多大
//   2. _primary_supers[]: 快速子类型检查（8 层以内直接查表）
//   3. _name: 类名
//   4. _super: 父类
//   5. _access_flags: 访问修饰符
//   6. _vtable_len: Java 虚方法表长度
//
// Layout Helper 编码：
//   - 正数 = 实例大小（字节，已对齐）
//   - 负数 = 数组描述符 [tag, hsz, ebt, log2(esz)]
//   - 零 = neutral（既不是实例也不是数组）
//
// Klass 层次：
//   Klass (Metadata)
//   ├── InstanceKlass              → 普通 Java 类
//   │   ├── InstanceMirrorKlass    → java.lang.Class
//   │   ├── InstanceClassLoaderKlass
//   │   └── InstanceRefKlass
//   └── ArrayKlass                 → 数组类
//       ├── ObjArrayKlass
//       └── TypeArrayKlass
// ============================================================================

#include "oops/metadata.hpp"
#include "oops/markOop.hpp"
#include "utilities/accessFlags.hpp"

// Klass 子类标识枚举
// HotSpot: klass.hpp 约 41-48 行
// 用于实现去虚化的 oop closure 派发
enum KlassID {
    InstanceKlassID,
    InstanceRefKlassID,
    InstanceMirrorKlassID,
    InstanceClassLoaderKlassID,
    TypeArrayKlassID,
    ObjArrayKlassID
};

const int KLASS_ID_COUNT = 6;

// 前向声明
class InstanceKlass;

class Klass : public Metadata {
    friend class InstanceKlass;

protected:
    // ========== 核心字段 ==========
    // HotSpot: klass.hpp 约 81-166 行
    // 按频繁访问程度排列（缓存友好）

    enum { _primary_super_limit = 8 };

    // Layout helper — 对象大小/数组描述符
    // 正数 = instance size in bytes（已对齐）
    // 负数 = array descriptor [tag | hsz | ebt | log2(esz)]
    // 零 = neutral
    jint        _layout_helper;

    // KlassID — 子类标识
    const KlassID _id;

    // 快速子类型检查用的偏移
    juint       _super_check_offset;

    // 类名（如 "java/lang/String"）
    // HotSpot 中是 Symbol*，我们暂用 char*
    const char* _name;

    // 主要父类列表（最多 8 层），用于快速子类型检查
    // _primary_supers[0] = java.lang.Object
    // _primary_supers[1] = 第一层子类
    // ...
    // _primary_supers[depth] = this
    Klass*      _primary_supers[_primary_super_limit];

    // 父类
    Klass*      _super;

    // 子类链表
    Klass*      _subklass;
    Klass*      _next_sibling;

    // 访问标志
    AccessFlags _access_flags;

    // 偏向锁原型头（用于偏向锁启用/禁用时的初始 mark word）
    markOop     _prototype_header;

    // vtable 长度（虚方法表条目数）
    int         _vtable_len;

    // 修饰符标志（给 Class.getModifiers() 用的处理后版本）
    jint        _modifier_flags;

public:
    // ======== 构造 ========

    Klass(KlassID id)
        : _layout_helper(0),
          _id(id),
          _super_check_offset(0),
          _name(nullptr),
          _super(nullptr),
          _subklass(nullptr),
          _next_sibling(nullptr),
          _access_flags(),
          _prototype_header(markOopDesc::prototype()),
          _vtable_len(0),
          _modifier_flags(0)
    {
        for (int i = 0; i < _primary_super_limit; i++) {
            _primary_supers[i] = nullptr;
        }
    }

    virtual ~Klass() {}

    // ======== Metadata 虚函数 ========

    bool is_klass() const override { return true; }
    const char* internal_name() const override { return "Klass"; }

    // ======== Layout Helper ========
    // HotSpot: klass.hpp 约 89-112 行

    jint layout_helper() const { return _layout_helper; }
    void set_layout_helper(jint lh) { _layout_helper = lh; }

    // Layout helper 解码

    static const int _lh_neutral_value = 0;
    static const int _lh_instance_slow_path_bit = 0x01;

    // 数组 layout helper 的编码常量
    static const int _lh_log2_element_size_shift = 0;
    static const int _lh_log2_element_size_mask  = BitsPerLong - 1;  // 0x3F → 实际只用低几位
    static const int _lh_element_type_shift      = 8;
    static const int _lh_element_type_mask       = 0xFF;
    static const int _lh_header_size_shift       = 16;
    static const int _lh_header_size_mask        = 0xFF;
    static const int _lh_array_tag_shift         = 24;
    static const int _lh_array_tag_obj_value     = (int)0x80000000 >> 24; // 0x80 → -128
    static const int _lh_array_tag_type_value    = (int)0xC0000000 >> 24; // 0xC0 → -64 (unsigned)

    bool is_instance_klass() const { return _layout_helper > _lh_neutral_value; }
    bool is_array_klass()    const { return _layout_helper <  _lh_neutral_value; }

    static int instance_layout_helper(int size, bool slow_path_flag) {
        return (size | (slow_path_flag ? _lh_instance_slow_path_bit : 0));
    }

    // ======== KlassID ========

    KlassID id() const { return _id; }

    // ======== 类名 ========

    const char* name() const { return _name; }
    void set_name(const char* n) { _name = n; }

    // ======== 父类 ========

    Klass* super() const { return _super; }
    void set_super(Klass* s) { _super = s; }

    // ======== 子类链表 ========

    Klass* subklass() const { return _subklass; }
    Klass* next_sibling() const { return _next_sibling; }

    // ======== 访问标志 ========

    AccessFlags access_flags() const { return _access_flags; }
    void set_access_flags(AccessFlags flags) { _access_flags = flags; }

    bool is_public()    const { return _access_flags.is_public(); }
    bool is_final()     const { return _access_flags.is_final(); }
    bool is_interface() const { return _access_flags.is_interface(); }
    bool is_abstract()  const { return _access_flags.is_abstract(); }

    jint modifier_flags() const { return _modifier_flags; }
    void set_modifier_flags(jint flags) { _modifier_flags = flags; }

    // ======== Prototype Header（偏向锁）========

    markOop prototype_header() const { return _prototype_header; }
    void set_prototype_header(markOop h) { _prototype_header = h; }

    // ======== vtable ========

    int vtable_length() const { return _vtable_len; }
    void set_vtable_length(int len) { _vtable_len = len; }

    // ======== Primary Supers（快速子类型检查）========

    Klass* primary_super_of_depth(juint i) const {
        vm_assert(i < (juint)_primary_super_limit, "primary super index out of bounds");
        return _primary_supers[i];
    }

    void set_primary_super(juint i, Klass* k) {
        vm_assert(i < (juint)_primary_super_limit, "primary super index out of bounds");
        _primary_supers[i] = k;
    }

    juint super_check_offset() const { return _super_check_offset; }
    void set_super_check_offset(juint offset) { _super_check_offset = offset; }

    // ======== 对象大小 ========

    // 获取实例大小（字节数，仅对 instance klass 有效）
    int size_helper() const {
        vm_assert(is_instance_klass(), "not an instance klass");
        return _layout_helper;
    }

    // ======== 调试打印 ========

    void print_on(FILE* out) const override {
        fprintf(out, "Klass(%p): name=\"%s\", layout_helper=%d, vtable_len=%d, super=%p",
                (void*)this,
                _name ? _name : "<null>",
                _layout_helper,
                _vtable_len,
                (void*)_super);
    }

    NONCOPYABLE(Klass);
};

#endif // SHARE_OOPS_KLASS_HPP
