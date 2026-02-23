#ifndef SHARE_UTILITIES_ACCESSFLAGS_HPP
#define SHARE_UTILITIES_ACCESSFLAGS_HPP

// ============================================================================
// Mini JVM - 访问标志
// 对应 HotSpot: src/hotspot/share/utilities/accessFlags.hpp
//
// AccessFlags 封装了 Java 访问标志（32 位 jint），包含：
//   - Java 标准标志（低 15 位）：public, private, static, final 等
//   - HotSpot 内部标志（高位）：分为 Method、Klass、Field 三类
//
// 关键设计：
//   1. 同一个 jint 被 Method/Klass/Field 在不同上下文中复用高位
//      例如 JVM_ACC_MONITOR_MATCH (Method) 和 JVM_ACC_HAS_MIRANDA_METHODS (Klass)
//      值相同 (0x10000000)，但不会冲突因为它们属于不同对象
//   2. atomic_set_bits/atomic_clear_bits 提供线程安全的标志修改
//      HotSpot 用 Atomic::cmpxchg，我们暂用普通赋值
// ============================================================================

#include "utilities/globalDefinitions.hpp"

// ----------------------------------------------------------------------------
// Java 标准访问标志常量
// 来自 JVM Spec Table 4.1-A, 4.5-A, 4.6-A
// HotSpot 中定义在 jvm.h，我们内联定义
// ----------------------------------------------------------------------------

// 类/接口/字段/方法 共用的标准标志
const jint JVM_ACC_PUBLIC       = 0x0001;
const jint JVM_ACC_PRIVATE      = 0x0002;
const jint JVM_ACC_PROTECTED    = 0x0004;
const jint JVM_ACC_STATIC       = 0x0008;
const jint JVM_ACC_FINAL        = 0x0010;
const jint JVM_ACC_SYNCHRONIZED = 0x0020;   // 方法
const jint JVM_ACC_SUPER        = 0x0020;   // 类（和 SYNCHRONIZED 同值，不同上下文）
const jint JVM_ACC_VOLATILE     = 0x0040;   // 字段
const jint JVM_ACC_BRIDGE       = 0x0040;   // 方法
const jint JVM_ACC_TRANSIENT    = 0x0080;   // 字段
const jint JVM_ACC_VARARGS      = 0x0080;   // 方法
const jint JVM_ACC_NATIVE       = 0x0100;
const jint JVM_ACC_INTERFACE    = 0x0200;
const jint JVM_ACC_ABSTRACT     = 0x0400;
const jint JVM_ACC_STRICT       = 0x0800;   // strictfp
const jint JVM_ACC_SYNTHETIC    = 0x1000;
const jint JVM_ACC_ANNOTATION   = 0x2000;
const jint JVM_ACC_ENUM         = 0x4000;

// .class 文件中实际写入的标志掩码（低 15 位）
const jint JVM_ACC_WRITTEN_FLAGS = 0x00007FFF;

// ----------------------------------------------------------------------------
// HotSpot 扩展标志 — Method 专用（高位）
// ----------------------------------------------------------------------------

const jint JVM_ACC_MONITOR_MATCH         = 0x10000000;  // monitorenter/monitorexit 匹配
const jint JVM_ACC_HAS_MONITOR_BYTECODES = 0x20000000;  // 包含 monitor 字节码
const jint JVM_ACC_HAS_LOOPS             = 0x40000000;  // 有循环
const jint JVM_ACC_LOOPS_FLAG_INIT       = (jint)0x80000000; // 循环标志已初始化
const jint JVM_ACC_QUEUED                = 0x01000000;  // 排队编译
const jint JVM_ACC_NOT_C2_COMPILABLE     = 0x02000000;
const jint JVM_ACC_NOT_C1_COMPILABLE     = 0x04000000;
const jint JVM_ACC_NOT_C2_OSR_COMPILABLE = 0x08000000;
const jint JVM_ACC_HAS_LINE_NUMBER_TABLE = 0x00100000;
const jint JVM_ACC_HAS_CHECKED_EXCEPTIONS= 0x00400000;
const jint JVM_ACC_HAS_JSRS              = 0x00800000;
const jint JVM_ACC_IS_OLD                = 0x00010000;  // 被 RedefineClasses 替换
const jint JVM_ACC_IS_OBSOLETE           = 0x00020000;
const jint JVM_ACC_IS_PREFIXED_NATIVE    = 0x00040000;
const jint JVM_ACC_ON_STACK              = 0x00080000;
const jint JVM_ACC_IS_DELETED            = 0x00008000;

// ----------------------------------------------------------------------------
// HotSpot 扩展标志 — Klass 专用（高位，和 Method 共享数值但不冲突）
// ----------------------------------------------------------------------------

const jint JVM_ACC_HAS_MIRANDA_METHODS     = 0x10000000;
const jint JVM_ACC_HAS_VANILLA_CONSTRUCTOR = 0x20000000;
const jint JVM_ACC_HAS_FINALIZER           = 0x40000000;
const jint JVM_ACC_IS_CLONEABLE_FAST       = (jint)0x80000000;
const jint JVM_ACC_HAS_FINAL_METHOD        = 0x01000000;
const jint JVM_ACC_IS_SHARED_CLASS         = 0x02000000;

// Klass 和 Method 共享
const jint JVM_ACC_HAS_LOCAL_VARIABLE_TABLE = 0x00200000;
const jint JVM_ACC_PROMOTED_FLAGS           = 0x00200000;

// ----------------------------------------------------------------------------
// HotSpot 扩展标志 — Field 专用（低 16 位中使用）
// ----------------------------------------------------------------------------

const jint JVM_ACC_FIELD_ACCESS_WATCHED           = 0x00002000;
const jint JVM_ACC_FIELD_MODIFICATION_WATCHED     = 0x00008000;
const jint JVM_ACC_FIELD_INTERNAL                 = 0x00000400;
const jint JVM_ACC_FIELD_STABLE                   = 0x00000020;
const jint JVM_ACC_FIELD_INITIALIZED_FINAL_UPDATE = 0x00000100;
const jint JVM_ACC_FIELD_HAS_GENERIC_SIGNATURE    = 0x00000800;

// ============================================================================
// AccessFlags 类
// HotSpot: accessFlags.hpp 约 102-288 行
//
// 封装一个 jint _flags，提供各种 is_xxx() 查询和 set_xxx() 修改方法。
// ============================================================================

class AccessFlags {
private:
    jint _flags;

public:
    AccessFlags() : _flags(0) {}
    explicit AccessFlags(jint flags) : _flags(flags) {}

    // ======== Java 标准标志查询 ========

    bool is_public()       const { return (_flags & JVM_ACC_PUBLIC)       != 0; }
    bool is_private()      const { return (_flags & JVM_ACC_PRIVATE)      != 0; }
    bool is_protected()    const { return (_flags & JVM_ACC_PROTECTED)    != 0; }
    bool is_static()       const { return (_flags & JVM_ACC_STATIC)       != 0; }
    bool is_final()        const { return (_flags & JVM_ACC_FINAL)        != 0; }
    bool is_synchronized() const { return (_flags & JVM_ACC_SYNCHRONIZED) != 0; }
    bool is_super()        const { return (_flags & JVM_ACC_SUPER)        != 0; }
    bool is_volatile()     const { return (_flags & JVM_ACC_VOLATILE)     != 0; }
    bool is_transient()    const { return (_flags & JVM_ACC_TRANSIENT)    != 0; }
    bool is_native()       const { return (_flags & JVM_ACC_NATIVE)       != 0; }
    bool is_interface()    const { return (_flags & JVM_ACC_INTERFACE)     != 0; }
    bool is_abstract()     const { return (_flags & JVM_ACC_ABSTRACT)     != 0; }
    bool is_strict()       const { return (_flags & JVM_ACC_STRICT)       != 0; }
    bool is_synthetic()    const { return (_flags & JVM_ACC_SYNTHETIC)    != 0; }

    // ======== Method 专用标志查询 ========

    bool has_monitor_bytecodes() const { return (_flags & JVM_ACC_HAS_MONITOR_BYTECODES) != 0; }
    bool has_loops()             const { return (_flags & JVM_ACC_HAS_LOOPS)              != 0; }
    bool has_linenumber_table()  const { return (_flags & JVM_ACC_HAS_LINE_NUMBER_TABLE)  != 0; }
    bool has_checked_exceptions()const { return (_flags & JVM_ACC_HAS_CHECKED_EXCEPTIONS) != 0; }
    bool has_localvariable_table() const { return (_flags & JVM_ACC_HAS_LOCAL_VARIABLE_TABLE) != 0; }

    // ======== Klass 专用标志查询 ========

    bool has_miranda_methods()     const { return (_flags & JVM_ACC_HAS_MIRANDA_METHODS)    != 0; }
    bool has_vanilla_constructor() const { return (_flags & JVM_ACC_HAS_VANILLA_CONSTRUCTOR) != 0; }
    bool has_finalizer()           const { return (_flags & JVM_ACC_HAS_FINALIZER)           != 0; }
    bool is_cloneable_fast()       const { return (_flags & JVM_ACC_IS_CLONEABLE_FAST)       != 0; }
    bool has_final_method()        const { return (_flags & JVM_ACC_HAS_FINAL_METHOD)        != 0; }

    // ======== 设置/修改 ========

    // 获取 .class 文件中的原始标志
    jint get_flags() const { return (_flags & JVM_ACC_WRITTEN_FLAGS); }

    // 设置标志（仅 .class 文件标志）
    void set_flags(jint flags) { _flags = (flags & JVM_ACC_WRITTEN_FLAGS); }

    // 原子设置位（简化版：非原子，后续可改用 CAS）
    void atomic_set_bits(jint bits)   { _flags |= bits; }
    void atomic_clear_bits(jint bits) { _flags &= ~bits; }

    // 便捷 setter
    void set_has_finalizer()           { atomic_set_bits(JVM_ACC_HAS_FINALIZER); }
    void set_has_final_method()        { atomic_set_bits(JVM_ACC_HAS_FINAL_METHOD); }
    void set_has_vanilla_constructor() { atomic_set_bits(JVM_ACC_HAS_VANILLA_CONSTRUCTOR); }
    void set_has_miranda_methods()     { atomic_set_bits(JVM_ACC_HAS_MIRANDA_METHODS); }
    void set_is_cloneable_fast()       { atomic_set_bits(JVM_ACC_IS_CLONEABLE_FAST); }
    void set_has_linenumber_table()    { atomic_set_bits(JVM_ACC_HAS_LINE_NUMBER_TABLE); }
    void set_has_checked_exceptions()  { atomic_set_bits(JVM_ACC_HAS_CHECKED_EXCEPTIONS); }
    void set_has_localvariable_table() { atomic_set_bits(JVM_ACC_HAS_LOCAL_VARIABLE_TABLE); }

    // 类型转换
    jshort as_short() const { return (jshort)_flags; }
    jint   as_int()   const { return _flags; }

    // 调试打印
    void print_on(FILE* out) const {
        fprintf(out, "0x%08X [", _flags);
        if (is_public())       fprintf(out, " public");
        if (is_private())      fprintf(out, " private");
        if (is_protected())    fprintf(out, " protected");
        if (is_static())       fprintf(out, " static");
        if (is_final())        fprintf(out, " final");
        if (is_synchronized()) fprintf(out, " synchronized");
        if (is_native())       fprintf(out, " native");
        if (is_interface())    fprintf(out, " interface");
        if (is_abstract())     fprintf(out, " abstract");
        fprintf(out, " ]");
    }
};

// 便捷工厂函数
inline AccessFlags accessFlags_from(jint flags) {
    return AccessFlags(flags);
}

#endif // SHARE_UTILITIES_ACCESSFLAGS_HPP
