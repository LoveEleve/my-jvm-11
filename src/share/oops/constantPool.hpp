#ifndef SHARE_OOPS_CONSTANTPOOL_HPP
#define SHARE_OOPS_CONSTANTPOOL_HPP

// ============================================================================
// Mini JVM - 常量池
// 对应 HotSpot: src/hotspot/share/oops/constantPool.hpp
//
// 常量池是 .class 文件中最核心的数据结构，保存了类中所有的：
//   - 字面量（整数、浮点数、字符串）
//   - 符号引用（类名、字段名、方法名、描述符）
//
// HotSpot 的 ConstantPool 继承 Metadata（元空间对象），内存布局是：
//   [ConstantPool 头部字段] [数据 slot 数组]
//   数据紧跟在对象之后，通过 base() 访问。
//
// 我们的精简版：
//   - 使用独立的数组 (_data) 存储 slot，不做内联布局
//   - 继承 CHeapObj<mtClass>（用 malloc 分配）
//   - 后续实现 Metaspace 后再迁移
//
// 每个常量池 slot 是一个 intptr_t (8 字节/LP64)：
//   - Integer/Float: 低 32 位存值
//   - Long/Double: 占 2 个 slot
//   - Class/String/FieldRef 等: 存索引或打包的索引对
//   - Utf8: 存 char* 指针（指向 UTF-8 字符串的堆拷贝）
// ============================================================================

#include "memory/allocation.hpp"
#include "utilities/constantTag.hpp"

class ConstantPool : public CHeapObj<mtClass> {
private:
    // ========== 核心字段 ==========

    u1*       _tags;       // 标签数组：_tags[i] 描述第 i 个 slot 的类型
    intptr_t* _data;       // 数据数组：每个 slot 一个 intptr_t
    int       _length;     // 常量池大小（slot 数量，含第 0 个空位）

    // 所属 InstanceKlass（回指）
    // HotSpot: _pool_holder
    void*     _pool_holder;  // 暂用 void* 避免循环依赖，实际是 InstanceKlass*

public:
    // ========== 构造/析构 ==========

    ConstantPool(int length)
        : _length(length),
          _pool_holder(nullptr) {
        // 分配标签数组，初始化为 Invalid
        _tags = NEW_C_HEAP_ARRAY(u1, length, mtClass);
        memset(_tags, JVM_CONSTANT_Invalid, length);

        // 分配数据数组，初始化为 0
        _data = NEW_C_HEAP_ARRAY(intptr_t, length, mtClass);
        memset(_data, 0, length * sizeof(intptr_t));
    }

    ~ConstantPool() {
        // 释放 Utf8 字符串
        for (int i = 1; i < _length; i++) {
            if (_tags[i] == JVM_CONSTANT_Utf8 && _data[i] != 0) {
                FREE_C_HEAP_ARRAY(char, (char*)_data[i]);
            }
        }
        FREE_C_HEAP_ARRAY(u1, _tags);
        FREE_C_HEAP_ARRAY(intptr_t, _data);
    }

    // ========== 基本查询 ==========

    int length() const { return _length; }

    // 所属 InstanceKlass
    void* pool_holder() const { return _pool_holder; }
    void set_pool_holder(void* holder) { _pool_holder = holder; }

    constantTag tag_at(int index) const {
        vm_assert(index >= 0 && index < _length, "index out of bounds");
        return constantTag(_tags[index]);
    }

    // ========== 写入方法（解析时使用）==========
    // HotSpot 中这些叫 xxx_at_put()

    // Utf8 字符串
    void utf8_at_put(int index, const u1* utf8_data, int utf8_length) {
        _tags[index] = JVM_CONSTANT_Utf8;
        // 拷贝 UTF-8 数据到堆上，追加 '\0' 便于 C 字符串使用
        char* copy = NEW_C_HEAP_ARRAY(char, utf8_length + 1, mtClass);
        memcpy(copy, utf8_data, utf8_length);
        copy[utf8_length] = '\0';
        _data[index] = (intptr_t)copy;
    }

    // Integer
    void int_at_put(int index, jint value) {
        _tags[index] = JVM_CONSTANT_Integer;
        _data[index] = (intptr_t)value;
    }

    // Float (用 memcpy 保留 bit pattern)
    void float_at_put(int index, jfloat value) {
        _tags[index] = JVM_CONSTANT_Float;
        jint bits;
        memcpy(&bits, &value, sizeof(jint));
        _data[index] = (intptr_t)bits;
    }

    // Long (占 2 个 slot)
    void long_at_put(int index, jlong value) {
        _tags[index] = JVM_CONSTANT_Long;
        // LP64 下一个 intptr_t 就够存 jlong
        _data[index] = (intptr_t)value;
        // index+1 标记为 Invalid（占位，不可单独访问）
        _tags[index + 1] = JVM_CONSTANT_Invalid;
    }

    // Double (占 2 个 slot)
    void double_at_put(int index, jdouble value) {
        _tags[index] = JVM_CONSTANT_Double;
        jlong bits;
        memcpy(&bits, &value, sizeof(jlong));
        _data[index] = (intptr_t)bits;
        _tags[index + 1] = JVM_CONSTANT_Invalid;
    }

    // Class (初始解析时只存 name_index)
    // HotSpot 先存为 ClassIndex(101)，后续改为 UnresolvedClass(100)
    void klass_index_at_put(int index, int name_index) {
        _tags[index] = JVM_CONSTANT_ClassIndex;
        _data[index] = (intptr_t)name_index;
    }

    // 将 ClassIndex 改为 UnresolvedClass
    void unresolved_klass_at_put(int index, int name_index) {
        _tags[index] = JVM_CONSTANT_UnresolvedClass;
        _data[index] = (intptr_t)name_index;
    }

    // String (存 utf8_index)
    void string_index_at_put(int index, int utf8_index) {
        _tags[index] = JVM_CONSTANT_StringIndex;
        _data[index] = (intptr_t)utf8_index;
    }

    // 将 StringIndex 改为 String
    void unresolved_string_at_put(int index, int utf8_index) {
        _tags[index] = JVM_CONSTANT_String;
        _data[index] = (intptr_t)utf8_index;
    }

    // Fieldref: 打包 class_index 和 name_and_type_index
    // HotSpot 把两个 u2 打包进一个 jint: 高16位 = name_type, 低16位 = class
    void field_at_put(int index, int class_index, int name_and_type_index) {
        _tags[index] = JVM_CONSTANT_Fieldref;
        _data[index] = ((intptr_t)name_and_type_index << 16) | class_index;
    }

    // Methodref
    void method_at_put(int index, int class_index, int name_and_type_index) {
        _tags[index] = JVM_CONSTANT_Methodref;
        _data[index] = ((intptr_t)name_and_type_index << 16) | class_index;
    }

    // InterfaceMethodref
    void interface_method_at_put(int index, int class_index, int name_and_type_index) {
        _tags[index] = JVM_CONSTANT_InterfaceMethodref;
        _data[index] = ((intptr_t)name_and_type_index << 16) | class_index;
    }

    // NameAndType
    void name_and_type_at_put(int index, int name_index, int signature_index) {
        _tags[index] = JVM_CONSTANT_NameAndType;
        _data[index] = ((intptr_t)signature_index << 16) | name_index;
    }

    // MethodHandle
    void method_handle_index_at_put(int index, int ref_kind, int method_index) {
        _tags[index] = JVM_CONSTANT_MethodHandle;
        _data[index] = ((intptr_t)method_index << 16) | ref_kind;
    }

    // MethodType
    void method_type_index_at_put(int index, int signature_index) {
        _tags[index] = JVM_CONSTANT_MethodType;
        _data[index] = (intptr_t)signature_index;
    }

    // InvokeDynamic
    void invoke_dynamic_at_put(int index, int bootstrap_method_attr_index, int name_and_type_index) {
        _tags[index] = JVM_CONSTANT_InvokeDynamic;
        _data[index] = ((intptr_t)name_and_type_index << 16) | bootstrap_method_attr_index;
    }

    // Dynamic (condy)
    void dynamic_constant_at_put(int index, int bootstrap_method_attr_index, int name_and_type_index) {
        _tags[index] = JVM_CONSTANT_Dynamic;
        _data[index] = ((intptr_t)name_and_type_index << 16) | bootstrap_method_attr_index;
    }

    // ========== 原始数据访问 ==========

    // 获取 raw slot 数据（解释器常量池解析用）
    intptr_t data_at(int index) const {
        vm_assert(index >= 0 && index < _length, "index out of bounds");
        return _data[index];
    }

    // ========== 读取方法 ==========

    // 获取 Utf8 字符串
    const char* utf8_at(int index) const {
        vm_assert(_tags[index] == JVM_CONSTANT_Utf8, "not a Utf8 entry");
        return (const char*)_data[index];
    }

    // 获取 Integer
    jint int_at(int index) const {
        vm_assert(_tags[index] == JVM_CONSTANT_Integer, "not an Integer entry");
        return (jint)_data[index];
    }

    // 获取 Float
    jfloat float_at(int index) const {
        vm_assert(_tags[index] == JVM_CONSTANT_Float, "not a Float entry");
        jint bits = (jint)_data[index];
        jfloat value;
        memcpy(&value, &bits, sizeof(jfloat));
        return value;
    }

    // 获取 Long
    jlong long_at(int index) const {
        vm_assert(_tags[index] == JVM_CONSTANT_Long, "not a Long entry");
        return (jlong)_data[index];
    }

    // 获取 Double
    jdouble double_at(int index) const {
        vm_assert(_tags[index] == JVM_CONSTANT_Double, "not a Double entry");
        jlong bits = (jlong)_data[index];
        jdouble value;
        memcpy(&value, &bits, sizeof(jdouble));
        return value;
    }

    // 获取 Class 的 name_index
    int klass_name_index_at(int index) const {
        vm_assert(_tags[index] == JVM_CONSTANT_ClassIndex ||
                  _tags[index] == JVM_CONSTANT_UnresolvedClass,
                  "not a Class entry");
        return (int)_data[index];
    }

    // 获取 String 的 utf8_index
    int string_utf8_index_at(int index) const {
        vm_assert(_tags[index] == JVM_CONSTANT_StringIndex ||
                  _tags[index] == JVM_CONSTANT_String,
                  "not a String entry");
        return (int)_data[index];
    }

    // 获取 Fieldref/Methodref/InterfaceMethodref 的 class_index
    int unchecked_klass_ref_index_at(int index) const {
        return (int)(_data[index] & 0xFFFF);
    }

    // 获取 Fieldref/Methodref/InterfaceMethodref 的 name_and_type_index
    int unchecked_name_and_type_ref_index_at(int index) const {
        return (int)((_data[index] >> 16) & 0xFFFF);
    }

    // NameAndType 的 name_index
    int name_ref_index_at(int index) const {
        vm_assert(_tags[index] == JVM_CONSTANT_NameAndType, "not a NameAndType entry");
        return (int)(_data[index] & 0xFFFF);
    }

    // NameAndType 的 signature_index
    int signature_ref_index_at(int index) const {
        vm_assert(_tags[index] == JVM_CONSTANT_NameAndType, "not a NameAndType entry");
        return (int)((_data[index] >> 16) & 0xFFFF);
    }

    // ========== 便捷方法 ==========

    // 通过 Class 条目获取类名字符串
    const char* klass_name_at(int index) const {
        int name_idx = klass_name_index_at(index);
        return utf8_at(name_idx);
    }

    // 通过 NameAndType 获取名称字符串
    const char* name_at(int name_and_type_index) const {
        int name_idx = name_ref_index_at(name_and_type_index);
        return utf8_at(name_idx);
    }

    // 通过 NameAndType 获取描述符字符串
    const char* signature_at(int name_and_type_index) const {
        int sig_idx = signature_ref_index_at(name_and_type_index);
        return utf8_at(sig_idx);
    }

    // ========== 调试打印 ==========

    void print_on(FILE* out) const;

    NONCOPYABLE(ConstantPool);
};

#endif // SHARE_OOPS_CONSTANTPOOL_HPP
