#ifndef SHARE_UTILITIES_CONSTANTTAG_HPP
#define SHARE_UTILITIES_CONSTANTTAG_HPP

// ============================================================================
// Mini JVM - 常量池标签定义
// 对应 HotSpot: src/hotspot/share/utilities/constantTag.hpp
//              + classfile_constants.h.template
//
// 常量池是 .class 文件的核心数据结构，每个条目有一个 tag 标识类型。
// 这些 tag 值直接来自 JVM 规范（JVMS §4.4）。
// ============================================================================

#include "utilities/globalDefinitions.hpp"

// ----------------------------------------------------------------------------
// JVM 规范定义的常量池标签
// 来源：classfile_constants.h.template 第 95-113 行
// 这些值写死在 .class 文件中，不能更改
// ----------------------------------------------------------------------------

enum {
    JVM_CONSTANT_Utf8               = 1,   // UTF-8 编码的字符串
    JVM_CONSTANT_Unicode            = 2,   // 未使用（历史遗留）
    JVM_CONSTANT_Integer            = 3,   // int 字面量
    JVM_CONSTANT_Float              = 4,   // float 字面量
    JVM_CONSTANT_Long               = 5,   // long 字面量（占 2 个 slot）
    JVM_CONSTANT_Double             = 6,   // double 字面量（占 2 个 slot）
    JVM_CONSTANT_Class              = 7,   // 类或接口的符号引用
    JVM_CONSTANT_String             = 8,   // 字符串字面量
    JVM_CONSTANT_Fieldref           = 9,   // 字段引用
    JVM_CONSTANT_Methodref          = 10,  // 方法引用
    JVM_CONSTANT_InterfaceMethodref = 11,  // 接口方法引用
    JVM_CONSTANT_NameAndType        = 12,  // 名称和类型描述符
    // 13, 14 未使用
    JVM_CONSTANT_MethodHandle       = 15,  // 方法句柄 (JSR 292)
    JVM_CONSTANT_MethodType         = 16,  // 方法类型 (JSR 292)
    JVM_CONSTANT_Dynamic            = 17,  // 动态计算常量 (JDK 11)
    JVM_CONSTANT_InvokeDynamic      = 18,  // invokedynamic 调用点
    JVM_CONSTANT_ExternalMax        = 18,  // .class 文件中可能出现的最大标签值
};

// ----------------------------------------------------------------------------
// HotSpot 内部扩展标签
// 来源：constantTag.hpp 第 34-48 行
//
// 这些标签不会出现在 .class 文件中，而是 HotSpot 在解析过程中
// 内部使用的临时/状态标签。
//
// 为什么需要？
//   解析 .class 时，CONSTANT_Class 条目一开始只知道 name_index，
//   还没有实际解析出 Klass*。HotSpot 用 UnresolvedClass (100) 标记
//   "已读取但未解析"的状态，等到真正用到时才解析（lazy resolution）。
// ----------------------------------------------------------------------------

enum {
    JVM_CONSTANT_Invalid                = 0,    // 无效/未初始化

    JVM_CONSTANT_InternalMin            = 100,
    JVM_CONSTANT_UnresolvedClass        = 100,  // 已读取但未解析的类引用
    JVM_CONSTANT_ClassIndex             = 101,  // 构建期临时：类的 name_index
    JVM_CONSTANT_StringIndex            = 102,  // 构建期临时：字符串的 utf8_index
    JVM_CONSTANT_UnresolvedClassInError = 103,  // 解析失败的类
    JVM_CONSTANT_MethodHandleInError    = 104,  // 解析失败的方法句柄
    JVM_CONSTANT_MethodTypeInError      = 105,  // 解析失败的方法类型
    JVM_CONSTANT_DynamicInError         = 106,  // 解析失败的动态常量
    JVM_CONSTANT_InternalMax            = 106,
};

// ----------------------------------------------------------------------------
// constantTag 类 — 标签的包装类
// HotSpot: constantTag.hpp 中的 constantTag 类
// 提供各种 is_xxx() 判断方法
// ----------------------------------------------------------------------------

class constantTag {
private:
    jbyte _tag;

public:
    constantTag() : _tag(JVM_CONSTANT_Invalid) {}
    constantTag(jbyte tag) : _tag(tag) {}

    // 判断方法
    bool is_klass()             const { return _tag == JVM_CONSTANT_Class; }
    bool is_field()             const { return _tag == JVM_CONSTANT_Fieldref; }
    bool is_method()            const { return _tag == JVM_CONSTANT_Methodref; }
    bool is_interface_method()  const { return _tag == JVM_CONSTANT_InterfaceMethodref; }
    bool is_string()            const { return _tag == JVM_CONSTANT_String; }
    bool is_int()               const { return _tag == JVM_CONSTANT_Integer; }
    bool is_float()             const { return _tag == JVM_CONSTANT_Float; }
    bool is_long()              const { return _tag == JVM_CONSTANT_Long; }
    bool is_double()            const { return _tag == JVM_CONSTANT_Double; }
    bool is_name_and_type()     const { return _tag == JVM_CONSTANT_NameAndType; }
    bool is_utf8()              const { return _tag == JVM_CONSTANT_Utf8; }

    bool is_method_handle()     const { return _tag == JVM_CONSTANT_MethodHandle; }
    bool is_method_type()       const { return _tag == JVM_CONSTANT_MethodType; }
    bool is_dynamic_constant()  const { return _tag == JVM_CONSTANT_Dynamic; }
    bool is_invoke_dynamic()    const { return _tag == JVM_CONSTANT_InvokeDynamic; }

    bool is_invalid()           const { return _tag == JVM_CONSTANT_Invalid; }
    bool is_unresolved_klass()  const { return _tag == JVM_CONSTANT_UnresolvedClass; }
    bool is_klass_index()       const { return _tag == JVM_CONSTANT_ClassIndex; }
    bool is_string_index()      const { return _tag == JVM_CONSTANT_StringIndex; }

    // 是否是引用类型条目（需要解析的）
    bool is_klass_or_reference() const {
        return is_klass() || is_unresolved_klass() || is_klass_index();
    }

    // Long 和 Double 占 2 个 slot
    bool is_double_slot() const {
        return _tag == JVM_CONSTANT_Long || _tag == JVM_CONSTANT_Double;
    }

    jbyte value() const { return _tag; }

    // 调试输出
    const char* to_string() const {
        switch (_tag) {
            case JVM_CONSTANT_Invalid:           return "Invalid";
            case JVM_CONSTANT_Utf8:              return "Utf8";
            case JVM_CONSTANT_Integer:           return "Integer";
            case JVM_CONSTANT_Float:             return "Float";
            case JVM_CONSTANT_Long:              return "Long";
            case JVM_CONSTANT_Double:            return "Double";
            case JVM_CONSTANT_Class:             return "Class";
            case JVM_CONSTANT_String:            return "String";
            case JVM_CONSTANT_Fieldref:          return "Fieldref";
            case JVM_CONSTANT_Methodref:         return "Methodref";
            case JVM_CONSTANT_InterfaceMethodref:return "InterfaceMethodref";
            case JVM_CONSTANT_NameAndType:       return "NameAndType";
            case JVM_CONSTANT_MethodHandle:      return "MethodHandle";
            case JVM_CONSTANT_MethodType:        return "MethodType";
            case JVM_CONSTANT_Dynamic:           return "Dynamic";
            case JVM_CONSTANT_InvokeDynamic:     return "InvokeDynamic";
            case JVM_CONSTANT_UnresolvedClass:   return "UnresolvedClass";
            case JVM_CONSTANT_ClassIndex:        return "ClassIndex";
            case JVM_CONSTANT_StringIndex:       return "StringIndex";
            default:                             return "Unknown";
        }
    }
};

#endif // SHARE_UTILITIES_CONSTANTTAG_HPP
