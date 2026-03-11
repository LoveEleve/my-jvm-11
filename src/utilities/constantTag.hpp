/*
 * my_jvm - Constant pool tag definitions
 * 
 * 参考 OpenJDK 11:
 *   - src/java.base/share/native/include/classfile_constants.h.template
 *   - src/hotspot/share/utilities/constantTag.hpp
 * 
 * 简化版本，内联 JVM_CONSTANT_XXX 枚举，避免依赖 jvm.h
 */

#ifndef MY_JVM_UTILITIES_CONSTANTTAG_HPP
#define MY_JVM_UTILITIES_CONSTANTTAG_HPP

#include "utilities/globalDefinitions.hpp"

// ========== Class 文件常量池标签（来自 classfile_constants.h） ==========

enum {
  // 标准 class 文件常量池标签
  JVM_CONSTANT_Utf8                   = 1,
  JVM_CONSTANT_Unicode                = 2,  // 未使用
  JVM_CONSTANT_Integer                = 3,
  JVM_CONSTANT_Float                  = 4,
  JVM_CONSTANT_Long                   = 5,
  JVM_CONSTANT_Double                 = 6,
  JVM_CONSTANT_Class                  = 7,
  JVM_CONSTANT_String                 = 8,
  JVM_CONSTANT_Fieldref               = 9,
  JVM_CONSTANT_Methodref              = 10,
  JVM_CONSTANT_InterfaceMethodref     = 11,
  JVM_CONSTANT_NameAndType            = 12,
  JVM_CONSTANT_MethodHandle           = 15,  // JSR 292
  JVM_CONSTANT_MethodType             = 16,  // JSR 292
  JVM_CONSTANT_Dynamic                = 17,
  JVM_CONSTANT_InvokeDynamic          = 18,
  
  // HotSpot 内部标签
  JVM_CONSTANT_Invalid                = 0,     // 无效值
  JVM_CONSTANT_InternalMin            = 100,   // 内部标签最小值
  JVM_CONSTANT_UnresolvedClass        = 100,   // 未解析的类
  JVM_CONSTANT_ClassIndex             = 101,   // 类索引（临时）
  JVM_CONSTANT_StringIndex            = 102,   // 字符串索引（临时）
  JVM_CONSTANT_UnresolvedClassInError = 103,   // 解析错误的类
  JVM_CONSTANT_MethodHandleInError    = 104,   // 解析错误的方法句柄
  JVM_CONSTANT_MethodTypeInError      = 105,   // 解析错误的方法类型
  JVM_CONSTANT_DynamicInError         = 106,   // 解析错误的动态常量
  JVM_CONSTANT_InternalMax            = 106    // 内部标签最大值
};

// ========== JVM_CONSTANT_MethodHandle 子类型 ==========

enum {
  JVM_REF_getField         = 1,
  JVM_REF_getStatic        = 2,
  JVM_REF_putField         = 3,
  JVM_REF_putStatic        = 4,
  JVM_REF_invokeVirtual    = 5,
  JVM_REF_invokeStatic     = 6,
  JVM_REF_invokeSpecial    = 7,
  JVM_REF_newInvokeSpecial = 8,
  JVM_REF_invokeInterface  = 9
};

// ========== 常量池标签类 ==========

class constantTag {
 private:
  jbyte _tag;

 public:
  // 构造函数
  constantTag() : _tag(JVM_CONSTANT_Invalid) {}
  explicit constantTag(jbyte tag) : _tag(tag) {}

  // 基本类型判断
  bool is_klass() const             { return _tag == JVM_CONSTANT_Class; }
  bool is_field() const             { return _tag == JVM_CONSTANT_Fieldref; }
  bool is_method() const            { return _tag == JVM_CONSTANT_Methodref; }
  bool is_interface_method() const  { return _tag == JVM_CONSTANT_InterfaceMethodref; }
  bool is_string() const            { return _tag == JVM_CONSTANT_String; }
  bool is_int() const               { return _tag == JVM_CONSTANT_Integer; }
  bool is_float() const             { return _tag == JVM_CONSTANT_Float; }
  bool is_long() const              { return _tag == JVM_CONSTANT_Long; }
  bool is_double() const            { return _tag == JVM_CONSTANT_Double; }
  bool is_name_and_type() const     { return _tag == JVM_CONSTANT_NameAndType; }
  bool is_utf8() const              { return _tag == JVM_CONSTANT_Utf8; }
  bool is_invalid() const           { return _tag == JVM_CONSTANT_Invalid; }

  // 方法句柄和动态常量
  bool is_method_type() const       { return _tag == JVM_CONSTANT_MethodType; }
  bool is_method_handle() const     { return _tag == JVM_CONSTANT_MethodHandle; }
  bool is_dynamic_constant() const  { return _tag == JVM_CONSTANT_Dynamic; }
  bool is_invoke_dynamic() const    { return _tag == JVM_CONSTANT_InvokeDynamic; }

  // 错误状态判断
  bool is_unresolved_klass() const {
    return _tag == JVM_CONSTANT_UnresolvedClass || _tag == JVM_CONSTANT_UnresolvedClassInError;
  }
  bool is_unresolved_klass_in_error() const { return _tag == JVM_CONSTANT_UnresolvedClassInError; }
  bool is_method_handle_in_error() const    { return _tag == JVM_CONSTANT_MethodHandleInError; }
  bool is_method_type_in_error() const      { return _tag == JVM_CONSTANT_MethodTypeInError; }
  bool is_dynamic_constant_in_error() const { return _tag == JVM_CONSTANT_DynamicInError; }

  // 索引类型
  bool is_klass_index() const  { return _tag == JVM_CONSTANT_ClassIndex; }
  bool is_string_index() const { return _tag == JVM_CONSTANT_StringIndex; }

  // 组合判断
  bool is_klass_reference() const    { return is_klass_index() || is_unresolved_klass(); }
  bool is_klass_or_reference() const { return is_klass() || is_klass_reference(); }
  bool is_field_or_method() const    { return is_field() || is_method() || is_interface_method(); }
  bool is_symbol() const             { return is_utf8(); }

  // 可加载常量
  bool is_loadable_constant() const {
    return ((_tag >= JVM_CONSTANT_Integer && _tag <= JVM_CONSTANT_String) ||
            is_method_type() || is_method_handle() || is_dynamic_constant() ||
            is_unresolved_klass());
  }

  // 获取值
  jbyte value() const { return _tag; }
};

#endif // MY_JVM_UTILITIES_CONSTANTTAG_HPP
