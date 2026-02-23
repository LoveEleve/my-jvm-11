#ifndef SHARE_CLASSFILE_CLASSFILEPARSER_HPP
#define SHARE_CLASSFILE_CLASSFILEPARSER_HPP

// ============================================================================
// Mini JVM - Class 文件解析器
// 对应 HotSpot: src/hotspot/share/classfile/classFileParser.hpp
//
// 这是 JVM 的核心组件之一。
// 职责：读取 .class 文件字节流，解析出类的所有信息。
//
// HotSpot 的 ClassFileParser 非常复杂（550+ 行头文件，6400+ 行实现），
// 因为它要处理：
//   - 完整的格式验证 (verify)
//   - 注解解析
//   - 字段布局计算 (layout_fields)
//   - 虚表/接口表大小计算
//   - 匿名类支持
//   - CDS 共享归档
//   - 各种 JVM 版本的兼容性
//
// 我们的精简版只保留核心解析流程：
//   magic → version → constant_pool → access_flags → this/super/interfaces
//   → fields → methods → attributes
//
// 当前阶段重点：解析到常量池 + 基本类信息
// 后续 Phase 再完善 fields/methods 的详细解析
// ============================================================================

#include "classfile/classFileStream.hpp"
#include "oops/constantPool.hpp"
#include "utilities/accessFlags.hpp"

// 前向声明
class InstanceKlass;
class Method;

// 简单的字段信息结构
struct FieldInfo {
    u2 access_flags;
    u2 name_index;
    u2 descriptor_index;
    u2 attributes_count;
    // 字段属性中的 ConstantValue 索引（如果有）
    u2 constant_value_index;

    const char* name(const ConstantPool* cp) const {
        return cp->utf8_at(name_index);
    }
    const char* descriptor(const ConstantPool* cp) const {
        return cp->utf8_at(descriptor_index);
    }
};

// 简单的方法信息结构
struct MethodInfo {
    u2 access_flags;
    u2 name_index;
    u2 descriptor_index;
    u2 attributes_count;

    // Code 属性
    u2 max_stack;
    u2 max_locals;
    u4 code_length;
    u1* code;         // 字节码（堆分配拷贝）

    // 异常表
    u2 exception_table_length;

    const char* name(const ConstantPool* cp) const {
        return cp->utf8_at(name_index);
    }
    const char* descriptor(const ConstantPool* cp) const {
        return cp->utf8_at(descriptor_index);
    }

    MethodInfo() : access_flags(0), name_index(0), descriptor_index(0),
                   attributes_count(0), max_stack(0), max_locals(0),
                   code_length(0), code(nullptr), exception_table_length(0) {}
    ~MethodInfo() {
        if (code != nullptr) {
            FREE_C_HEAP_ARRAY(u1, code);
        }
    }
};

// ============================================================================
// ClassFileParser 类
// ============================================================================

class ClassFileParser : public CHeapObj<mtClass> {
private:
    // ========== 输入 ==========
    const ClassFileStream* _stream;

    // ========== 解析结果 ==========
    u4 _magic;
    u2 _minor_version;
    u2 _major_version;

    ConstantPool* _cp;           // 常量池

    u2 _access_flags;
    u2 _this_class_index;        // this_class 在常量池中的索引
    u2 _super_class_index;       // super_class 在常量池中的索引

    // 类名（从常量池中获取）
    const char* _class_name;
    const char* _super_class_name;

    // 接口
    int _interfaces_count;
    u2* _interfaces;             // 接口的常量池索引数组

    // 字段
    int _fields_count;
    FieldInfo* _fields;

    // 方法
    int _methods_count;
    MethodInfo* _methods;

    // ========== 私有解析方法 ==========

    // 按照 .class 文件结构顺序，对应 HotSpot parse_stream() 中的各步骤

    void parse_magic_and_version();
    void parse_constant_pool();
    void parse_constant_pool_entries(int length);
    void post_process_constant_pool();
    void parse_access_flags();
    void parse_this_class();
    void parse_super_class();
    void parse_interfaces();
    void parse_fields();
    void parse_methods();
    void parse_class_attributes();

    // 解析单个方法的 Code 属性
    void parse_method_code_attribute(MethodInfo* method);

    // 跳过不认识的属性
    void skip_attribute();

public:
    // ========== 构造/析构 ==========

    ClassFileParser(const ClassFileStream* stream);
    ~ClassFileParser();

    // ========== 主解析入口 ==========
    // 对应 HotSpot: ClassFileParser::parse_stream()
    void parse();

    // ========== Phase 3: 创建 InstanceKlass ==========
    // 对应 HotSpot: ClassFileParser::create_instance_klass()
    // 将解析结果转化为 InstanceKlass 对象
    InstanceKlass* create_instance_klass();

    // ========== 查询接口 ==========

    u2 major_version()       const { return _major_version; }
    u2 minor_version()       const { return _minor_version; }
    u2 access_flags()        const { return _access_flags; }
    const char* class_name() const { return _class_name; }
    const char* super_class_name() const { return _super_class_name; }

    const ConstantPool* constant_pool() const { return _cp; }

    int interfaces_count()   const { return _interfaces_count; }
    int fields_count()       const { return _fields_count; }
    int methods_count()      const { return _methods_count; }

    const FieldInfo*  field_at(int i)  const { return &_fields[i]; }
    const MethodInfo* method_at(int i) const { return &_methods[i]; }

    // 获取接口名
    const char* interface_name_at(int i) const {
        return _cp->klass_name_at(_interfaces[i]);
    }

    // ========== 调试打印 ==========
    void print_summary(FILE* out) const;

    NONCOPYABLE(ClassFileParser);
};

#endif // SHARE_CLASSFILE_CLASSFILEPARSER_HPP
