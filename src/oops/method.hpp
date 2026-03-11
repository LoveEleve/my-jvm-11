/*
 * my_jvm - Method (Method Metadata)
 *
 * 方法元数据，来自 OpenJDK 11 hotspot/src/share/vm/oops/method.hpp
 * 对标 OpenJDK 11 slowdebug 版本
 *
 * 内存布局（64-bit slowdebug）：
 *   [0]   Metadata base (vtable + _valid + padding) = 16 bytes
 *   [16]  _constMethod: ConstMethod* (8)
 *   [24]  _method_data: MethodData* (8)
 *   [32]  _method_counters: MethodCounters* (8)
 *   [40]  _access_flags: AccessFlags (4)
 *   [44]  _vtable_index: int (4)
 *   [48]  _intrinsic_id: u2 (2)
 *   [50]  _flags: u2 (2)
 *   [52]  JFR trace flag: u2 (2) + padding (2)
 *   [56]  NOT_PRODUCT: _compiled_invocation_count: int64_t (8)
 *   [64]  _i2i_entry: address (8)
 *   [72]  _from_compiled_entry: address (8)
 *   [80]  _code: CompiledMethod* (8)
 *   [88]  _from_interpreted_entry: address (8)
 *   [96]  AOT: _aot_code (8) ← 仅 TIERED+AOT，slowdebug 无
 *   sizeof(Method) = 104  ← GDB 验证（无 AOT）
 *
 * 注意：Method 继承 Metadata，不是 oopDesc！
 */

#ifndef MY_JVM_OOPS_METHOD_HPP
#define MY_JVM_OOPS_METHOD_HPP

#include "globalDefinitions.hpp"
#include "metadata.hpp"

// ========== 前向声明 ==========

class ConstMethod;
class MethodData;
class MethodCounters;
class CompiledMethod;

// ========== Method 类 ==========
// 方法元数据，存在 Metaspace 中
// 参考：method.hpp 第 70-118 行

class Method : public Metadata {
private:
    // ========== 字段（严格按 OpenJDK 源码顺序）==========

    // 方法只读数据（字节码、常量池索引等）
    // 参考：method.hpp 第 76 行
    ConstMethod*      _constMethod;

    // 方法 profiling 数据（用于 JIT 优化）
    // 参考：method.hpp 第 77 行
    MethodData*       _method_data;

    // 方法计数器（调用次数、回边次数等）
    // 参考：method.hpp 第 78 行
    MethodCounters*   _method_counters;

    // 访问标志（public, private, static 等）
    // 参考：method.hpp 第 79 行
    // OpenJDK 中是 AccessFlags 类型（内部是 jint），这里简化为 u4
    u4                _access_flags;

    // vtable 索引（用于虚调用分发）
    // 参考：method.hpp 第 80 行
    // 负值有特殊含义（见 VtableIndexFlag 枚举）
    int               _vtable_index;

    // 内置方法 ID（0 表示非内置）
    // 参考：method.hpp 第 82 行
    u2                _intrinsic_id;

    // 各种标志位（caller_sensitive, force_inline 等）
    // 参考：method.hpp 第 85-95 行
    mutable u2        _flags;

    // JFR trace flag（简化为 u2）
    u2                _jfr_trace_flag;

    // padding to align next field
    u2                _padding;

    // NOT_PRODUCT: 编译调用计数（slowdebug 版本存在）
    // 参考：method.hpp 第 99 行
    int64_t           _compiled_invocation_count;

    // 解释器到解释器入口（所有参数在栈上的调用约定）
    // 参考：method.hpp 第 101 行
    address           _i2i_entry;

    // 从编译代码调用的入口（编译代码存在时指向编译代码，否则指向 c2i 适配器）
    // 参考：method.hpp 第 103 行
    volatile address  _from_compiled_entry;

    // 对应的本地编译代码（NULL 表示未编译）
    // 参考：method.hpp 第 108 行
    CompiledMethod* volatile _code;

    // 从解释器调用的入口缓存
    // 参考：method.hpp 第 110 行
    volatile address  _from_interpreted_entry;

    // AOT 编译代码（TIERED 模式下存在）
    // 参考：method.hpp 第 113 行（#if INCLUDE_AOT && defined(TIERED)）
    // slowdebug 版本包含此字段，sizeof(Method) = 104
    CompiledMethod*   _aot_code;

public:
    // ========== 构造函数 ==========

    Method() : _constMethod(nullptr), _method_data(nullptr),
               _method_counters(nullptr), _access_flags(0),
               _vtable_index(0), _intrinsic_id(0), _flags(0),
               _jfr_trace_flag(0), _padding(0),
               _compiled_invocation_count(0),
               _i2i_entry(0), _from_compiled_entry(0),
               _code(nullptr), _from_interpreted_entry(0),
               _aot_code(nullptr) {}

    // ========== 类型判断 ==========

    bool is_method() const override { return true; }

    // ========== ConstMethod 访问 ==========

    ConstMethod* constMethod() const { return _constMethod; }
    void set_constMethod(ConstMethod* cm) { _constMethod = cm; }

    // ========== 访问标志 ==========

    u4 access_flags() const { return _access_flags; }
    void set_access_flags(u4 flags) { _access_flags = flags; }

    bool is_public() const      { return (_access_flags & 0x0001) != 0; }
    bool is_private() const     { return (_access_flags & 0x0002) != 0; }
    bool is_protected() const   { return (_access_flags & 0x0004) != 0; }
    bool is_static() const      { return (_access_flags & 0x0008) != 0; }
    bool is_final() const       { return (_access_flags & 0x0010) != 0; }
    bool is_synchronized() const{ return (_access_flags & 0x0020) != 0; }
    bool is_native() const      { return (_access_flags & 0x0100) != 0; }
    bool is_abstract() const    { return (_access_flags & 0x0400) != 0; }

    // ========== vtable 索引 ==========

    // vtable 索引的特殊值（参考 method.hpp VtableIndexFlag）
    enum VtableIndexFlag {
        itable_index_max        = -10,
        pending_itable_index    = -9,
        invalid_vtable_index    = -4,
        garbage_vtable_index    = -3,
        nonvirtual_vtable_index = -2
    };

    int vtable_index() const { return _vtable_index; }
    void set_vtable_index(int index) { _vtable_index = index; }

    bool has_vtable_index() const { return _vtable_index >= 0; }
    bool has_itable_index() const { return _vtable_index <= itable_index_max; }

    // ========== 内置方法 ==========

    u2 intrinsic_id() const { return _intrinsic_id; }
    void set_intrinsic_id(u2 id) { _intrinsic_id = id; }

    // ========== 方法数据 ==========

    MethodData* method_data() const { return _method_data; }
    void set_method_data(MethodData* data) { _method_data = data; }

    MethodCounters* method_counters() const { return _method_counters; }
    void clear_method_counters() { _method_counters = nullptr; }

    // ========== 入口点 ==========

    address interpreter_entry() const { return _i2i_entry; }
    address from_compiled_entry() const { return _from_compiled_entry; }
    address from_interpreted_entry() const { return _from_interpreted_entry; }

    // ========== 编译代码 ==========

    CompiledMethod* volatile code() const { return _code; }

    // 消除 volatile 限定符警告
    address i2i_entry() const { return _i2i_entry; }
};


// ========== 类型别名 ==========

typedef Method* MethodPtr;

#endif // MY_JVM_OOPS_METHOD_HPP
