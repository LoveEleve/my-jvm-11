#ifndef SHARE_OOPS_METHOD_HPP
#define SHARE_OOPS_METHOD_HPP

// ============================================================================
// Mini JVM - Method（方法元数据）
// 对应 HotSpot: src/hotspot/share/oops/method.hpp
//
// Method 代表一个 Java 方法，包含：
//   - _constMethod: 不可变部分（字节码、行号表等）
//   - _access_flags: 访问标志
//   - _vtable_index: vtable 索引
//   - 入口点地址（解释器/编译器入口）
//
// 关键设计：
//   1. Method 继承 Metadata（有虚函数，在元空间分配）
//   2. 方法数据分为可变/不可变两部分：
//      - ConstMethod: 不可变（classfile 加载后不变），可跨进程共享 (CDS)
//      - Method: 可变（运行时更新入口点、编译状态等）
//   3. vtable_index 决定了虚调用时的派发位置
//      itable_index 存储为负数编码（itable 和 vtable 共用一个 int）
//   4. native 方法在 Method 末尾内嵌 native_function 和 signature_handler
//
// 内存布局：
//   ┌──────────────────────────────┐
//   │ Metadata vptr (8 B)          │  C++ 虚表指针
//   ├──────────────────────────────┤
//   │ _constMethod (ConstMethod*)  │  指向不可变部分
//   │ _access_flags (AccessFlags)  │  访问标志
//   │ _vtable_index (int)          │  虚表索引
//   │ _intrinsic_id (u2)           │  内建方法 ID
//   │ _flags (u2)                  │  内部标志
//   │ _i2i_entry (address)         │  解释器入口
//   │ _from_compiled_entry (addr)  │  编译代码入口
//   │ _code (CompiledMethod*)      │  编译后的代码
//   │ _from_interpreted_entry (addr)│ 解释器→编译代码入口
//   ├──────────────────────────────┤
//   │ [native_function]            │  仅 native 方法
//   │ [signature_handler]          │  仅 native 方法
//   └──────────────────────────────┘
// ============================================================================

#include "oops/metadata.hpp"
#include "oops/constMethod.hpp"
#include "utilities/accessFlags.hpp"

class InstanceKlass;

class Method : public Metadata {
private:
    // ========== 字段 ==========
    // HotSpot: method.hpp 约 73-113 行

    ConstMethod*    _constMethod;       // 不可变数据（字节码等）
    AccessFlags     _access_flags;      // 访问标志
    int             _vtable_index;      // vtable 索引

    u2              _intrinsic_id;      // 内建方法 ID（0 = 无）
    u2              _flags;             // 内部标志

    // 入口点
    address         _i2i_entry;                // 解释器 → 解释器
    volatile address _from_compiled_entry;     // 编译代码 → 编译代码/解释器
    volatile address _from_interpreted_entry;  // 解释器 → 编译代码/解释器

    // native 方法的函数指针（精简版：存为字段而非内嵌）
    address         _native_function;
    address         _signature_handler;

    // HotSpot 中还有 _method_data, _method_counters 等，后续添加

public:
    // ======== VtableIndex 特殊值 ========
    // HotSpot: method.hpp 中 VtableIndexFlag enum

    enum VtableIndexFlag {
        // 非虚方法（private, static, final, <init> 等）
        nonvirtual_vtable_index = -2,
        // itable 索引的编码：-(itable_index + pending_itable_resize_flag)
        pending_itable_resize_flag = -3,
        // 无效索引
        invalid_vtable_index = -4
    };

    // ======== Flags ========
    enum MethodFlags {
        _caller_sensitive      = 1 << 0,
        _force_inline          = 1 << 1,
        _dont_inline           = 1 << 2,
        _hidden                = 1 << 3,
        _has_injected_profile  = 1 << 4,
        _intrinsic_candidate   = 1 << 6,
        _reserved_stack_access = 1 << 7
    };

    // ======== 构造/析构 ========

    Method(ConstMethod* constMethod, AccessFlags access_flags)
        : _constMethod(constMethod),
          _access_flags(access_flags),
          _vtable_index(invalid_vtable_index),
          _intrinsic_id(0),
          _flags(0),
          _i2i_entry(nullptr),
          _from_compiled_entry(nullptr),
          _from_interpreted_entry(nullptr),
          _native_function(nullptr),
          _signature_handler(nullptr)
    {}

    ~Method() {
        if (_constMethod != nullptr) {
            delete _constMethod;
        }
    }

    // ======== Metadata 虚函数 ========

    bool is_method() const override { return true; }
    const char* internal_name() const override { return "Method"; }

    // ======== ConstMethod 访问 ========

    ConstMethod* constMethod() const          { return _constMethod; }
    void set_constMethod(ConstMethod* cm)     { _constMethod = cm; }

    // ======== 常量池（通过 ConstMethod 间接访问）========

    ConstantPool* constants() const { return _constMethod->constants(); }

    // ======== 字节码访问 ========

    u2  code_size()  const { return _constMethod->code_size(); }
    u1* code_base()  const { return _constMethod->code_base(); }

    // ======== 方法名/签名（常量池索引）========

    u2 name_index()      const { return _constMethod->name_index(); }
    u2 signature_index() const { return _constMethod->signature_index(); }

    // ======== 栈/局部变量 ========

    u2 max_stack()  const { return _constMethod->max_stack(); }
    u2 max_locals() const { return _constMethod->max_locals(); }

    u2 size_of_parameters() const { return _constMethod->size_of_parameters(); }
    void set_size_of_parameters(u2 v) { _constMethod->set_size_of_parameters(v); }

    // ======== 访问标志 ========

    AccessFlags access_flags() const          { return _access_flags; }
    void set_access_flags(AccessFlags flags)  { _access_flags = flags; }

    bool is_public()       const { return _access_flags.is_public(); }
    bool is_private()      const { return _access_flags.is_private(); }
    bool is_protected()    const { return _access_flags.is_protected(); }
    bool is_static()       const { return _access_flags.is_static(); }
    bool is_final()        const { return _access_flags.is_final(); }
    bool is_synchronized() const { return _access_flags.is_synchronized(); }
    bool is_native()       const { return _access_flags.is_native(); }
    bool is_abstract()     const { return _access_flags.is_abstract(); }

    // ======== vtable 索引 ========

    int vtable_index() const { return _vtable_index; }
    void set_vtable_index(int index) { _vtable_index = index; }

    // ======== 入口点 ========

    address i2i_entry()              const { return _i2i_entry; }
    address from_compiled_entry()    const { return _from_compiled_entry; }
    address from_interpreted_entry() const { return _from_interpreted_entry; }

    void set_i2i_entry(address entry)              { _i2i_entry = entry; }
    void set_from_compiled_entry(address entry)    { _from_compiled_entry = entry; }
    void set_from_interpreted_entry(address entry) { _from_interpreted_entry = entry; }

    // ======== Native 方法 ========

    bool is_native_method() const { return _access_flags.is_native(); }

    address native_function() const { return _native_function; }
    void set_native_function(address fn) { _native_function = fn; }

    address signature_handler() const { return _signature_handler; }
    void set_signature_handler(address sh) { _signature_handler = sh; }

    // ======== 方法编号 ========

    u2 method_idnum() const { return _constMethod->method_idnum(); }
    void set_method_idnum(u2 v) { _constMethod->set_method_idnum(v); }

    // ======== 调试打印 ========

    void print_on(FILE* out) const override {
        fprintf(out, "Method(%p): name_idx=%d, sig_idx=%d, flags=",
                (void*)this, name_index(), signature_index());
        _access_flags.print_on(out);
        fprintf(out, ", vtable_index=%d", _vtable_index);
        fprintf(out, ", code_size=%d, max_stack=%d, max_locals=%d",
                code_size(), max_stack(), max_locals());
    }

    NONCOPYABLE(Method);
};

#endif // SHARE_OOPS_METHOD_HPP
