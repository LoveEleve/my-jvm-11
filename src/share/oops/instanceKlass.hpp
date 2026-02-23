#ifndef SHARE_OOPS_INSTANCEKLASS_HPP
#define SHARE_OOPS_INSTANCEKLASS_HPP

// ============================================================================
// Mini JVM - InstanceKlass（Java 类的完整元数据）
// 对应 HotSpot: src/hotspot/share/oops/instanceKlass.hpp
//
// InstanceKlass 是 Klass 的子类，表示一个具体的 Java 类（非数组）。
// 包含运行一个类所需的所有信息：
//   - _constants: 常量池
//   - _methods: 方法数组
//   - _fields: 字段描述
//   - _vtable_len / _itable_len: 虚表/接口表长度
//   - ClassState: 类的加载状态
//   等等
//
// ClassState 状态机：
//   allocated → loaded → linked → being_initialized → fully_initialized
//                                                   → initialization_error
//
// HotSpot 中 InstanceKlass 末尾有内嵌数据：
//   [EMBEDDED Java vtable]             vtable_len 个条目
//   [EMBEDDED nonstatic oop-map blocks] 描述实例中 oop 字段的位置
//   [EMBEDDED implementor]             仅接口有
//
// 我们的精简版：
//   - vtable/itable 暂用独立数组（后续改为内嵌）
//   - 不实现 oop-map blocks（后续 GC 需要时添加）
//   - 字段信息暂用简单数组
//
// OopMapBlock 说明（预留，后续 GC 用）：
//   描述实例中哪些偏移位置有 oop 指针，GC 需要这个信息来扫描对象引用。
//   每个 block 是 (offset, count) 对，表示从 offset 开始连续 count 个 oop。
// ============================================================================

#include "oops/klass.hpp"
#include "oops/constantPool.hpp"
#include "oops/method.hpp"
#include "oops/instanceOop.hpp"

// OopMapBlock — 描述实例中 oop 字段的位置
// HotSpot: instanceKlass.hpp 约 93-112 行
class OopMapBlock {
public:
    int offset() const          { return _offset; }
    void set_offset(int offset) { _offset = offset; }

    int count() const          { return _count; }
    void set_count(int count)  { _count = count; }

    static int size_in_words() {
        return (int)align_up((uintx)sizeof(OopMapBlock), (uintx)wordSize) / wordSize;
    }

private:
    int _offset;
    int _count;
};

// 简化的字段描述
// HotSpot 的 FieldInfo 存储在 Array<u2> 中，每个字段 6 个 u2
// 我们暂用结构体
struct FieldInfoEntry {
    u2 access_flags;
    u2 name_index;
    u2 descriptor_index;
    u2 offset;           // 字段在实例中的字节偏移（static 字段为 invalid_offset）
    u2 constant_value_index;  // ConstantValue 属性（static final 字段）

    static const u2 invalid_offset = 0xFFFF;  // static 字段无实例偏移

    bool is_static() const { return (access_flags & JVM_ACC_STATIC) != 0; }
};

class InstanceKlass : public Klass {
public:
    static const KlassID ID = InstanceKlassID;

    // ========== 类状态 ==========
    // HotSpot: instanceKlass.hpp 约 133-140 行

    enum ClassState {
        allocated,            // 已分配，尚未链接
        loaded,               // 已加载并插入类层次
        linked,               // 已链接/验证
        being_initialized,    // 正在运行 <clinit>
        fully_initialized,    // 初始化完成
        initialization_error  // 初始化出错
    };

private:
    // ========== 字段 ==========
    // HotSpot: instanceKlass.hpp 约 148-250 行

    // 常量池（回指）
    ConstantPool* _constants;

    // 方法数组
    int       _methods_count;
    Method**  _methods;         // Method* 数组

    // 字段描述数组
    int             _fields_count;
    FieldInfoEntry* _field_infos;   // 字段信息数组

    // 接口
    int       _local_interfaces_count;
    Klass**   _local_interfaces;  // 直接实现的接口

    // 类的状态
    ClassState _init_state;

    // 大小信息
    int _nonstatic_field_size;     // 非静态字段占用的 heapOopSize words
    int _static_field_size;        // 静态字段占用的 words
    int _nonstatic_oop_map_size;   // oop-map blocks 的大小（words）

    u2  _java_fields_count;        // Java 声明的字段数
    int _itable_len;               // itable 长度（words）

    // misc flags（HotSpot 的 _misc_flags）
    u2  _misc_flags;

    // 版本号
    u2  _minor_version;
    u2  _major_version;

    // 源文件名索引（常量池索引）
    u2  _source_file_name_index;

    // 类名（堆分配的拷贝，包含 '/' 分隔符）
    char* _class_name;

    // 父类名
    char* _super_class_name;

    // ========== misc_flags 位定义 ==========

    enum {
        _misc_rewritten                = 1 << 2,
        _misc_has_nonstatic_fields     = 1 << 3,
        _misc_should_verify_class      = 1 << 4,
        _misc_is_contended             = 1 << 6,
        _misc_has_nonstatic_concrete_methods = 1 << 7,
        _misc_declares_nonstatic_concrete_methods = 1 << 8,
    };

public:
    // ======== 构造/析构 ========

    InstanceKlass();
    ~InstanceKlass();

    // ======== Metadata 虚函数 ========

    const char* internal_name() const override { return "InstanceKlass"; }

    // ======== 类状态 ========

    ClassState init_state() const { return _init_state; }
    void set_init_state(ClassState state) { _init_state = state; }

    bool is_loaded()      const { return _init_state >= loaded; }
    bool is_linked()      const { return _init_state >= linked; }
    bool is_initialized() const { return _init_state == fully_initialized; }
    bool is_not_initialized() const { return _init_state < being_initialized; }
    bool is_being_initialized() const { return _init_state == being_initialized; }
    bool is_in_error_state() const { return _init_state == initialization_error; }

    // ======== 常量池 ========

    ConstantPool* constants() const { return _constants; }
    void set_constants(ConstantPool* cp) { _constants = cp; }

    // ======== 方法 ========

    int methods_count() const { return _methods_count; }
    Method** methods() const { return _methods; }

    Method* method_at(int index) const {
        vm_assert(index >= 0 && index < _methods_count, "method index out of bounds");
        return _methods[index];
    }

    void set_methods(Method** methods, int count) {
        _methods = methods;
        _methods_count = count;
    }

    // 根据名称和签名查找方法
    Method* find_method(const char* name, const char* signature) const;

    // ======== 字段 ========

    int fields_count() const { return _fields_count; }
    u2  java_fields_count() const { return _java_fields_count; }

    const FieldInfoEntry* field_info_at(int index) const {
        vm_assert(index >= 0 && index < _fields_count, "field index out of bounds");
        return &_field_infos[index];
    }

    void set_fields(FieldInfoEntry* fields, int count) {
        _field_infos = fields;
        _fields_count = count;
        _java_fields_count = (u2)count;
    }

    // ======== 接口 ========

    int local_interfaces_count() const { return _local_interfaces_count; }

    void set_local_interfaces(Klass** interfaces, int count) {
        _local_interfaces = interfaces;
        _local_interfaces_count = count;
    }

    // ======== 大小信息 ========

    int nonstatic_field_size() const { return _nonstatic_field_size; }
    void set_nonstatic_field_size(int size) { _nonstatic_field_size = size; }

    int static_field_size() const { return _static_field_size; }
    void set_static_field_size(int size) { _static_field_size = size; }

    int itable_len() const { return _itable_len; }
    void set_itable_len(int len) { _itable_len = len; }

    // ======== 版本号 ========

    u2 minor_version() const { return _minor_version; }
    u2 major_version() const { return _major_version; }
    void set_minor_version(u2 v) { _minor_version = v; }
    void set_major_version(u2 v) { _major_version = v; }

    // ======== 源文件 ========

    u2 source_file_name_index() const { return _source_file_name_index; }
    void set_source_file_name_index(u2 idx) { _source_file_name_index = idx; }

    // ======== 类名 ========

    const char* class_name() const { return _class_name; }
    const char* super_class_name() const { return _super_class_name; }

    void set_class_name(const char* name);
    void set_super_class_name(const char* name);

    // ======== misc flags ========

    bool has_nonstatic_fields() const {
        return (_misc_flags & _misc_has_nonstatic_fields) != 0;
    }
    void set_has_nonstatic_fields() {
        _misc_flags |= _misc_has_nonstatic_fields;
    }

    // ======== 实例大小 ========

    // 计算实例对象大小（字节数）
    // layout_helper 存储的就是实例大小（已经对齐到 HeapWord）
    int instance_size() const {
        vm_assert(is_instance_klass(), "not an instance klass");
        return layout_helper();
    }

    // 设置实例大小
    void set_instance_size(int size) {
        set_layout_helper(Klass::instance_layout_helper(size, false));
    }

    // ======== 工厂方法 ========

    // 从 ClassFileParser 创建 InstanceKlass（Phase 3 核心衔接点）
    static InstanceKlass* create_from_parser(
        const char* class_name,
        const char* super_class_name,
        u2 access_flags,
        u2 major_version,
        u2 minor_version,
        ConstantPool* cp,
        FieldInfoEntry* fields, int fields_count,
        Method** methods, int methods_count
    );

    // ======== 调试打印 ========

    void print_on(FILE* out) const override;
    void print_summary(FILE* out) const;

    NONCOPYABLE(InstanceKlass);
};

#endif // SHARE_OOPS_INSTANCEKLASS_HPP
