/*
 * my_jvm - InstanceKlass (Instance Class Metadata)
 *
 * 普通 Java 类的元数据，来自 OpenJDK 11 hotspot/src/share/vm/oops/instanceKlass.hpp
 * 对标 OpenJDK 11 slowdebug 版本
 *
 * 内存布局（64-bit slowdebug）：
 *   [0]    Klass base = 208 bytes
 *   [208]  InstanceKlass 自有字段开始
 *   ...
 *   sizeof(InstanceKlass) = 472  ← GDB 验证
 *
 * GDB 关键偏移：
 *   _vtable_len   = 196  (Klass 内部字段)
 *   _itable_len   = 308  (InstanceKlass 字段)
 *   _methods      = 416  (InstanceKlass 字段)
 *   _constants    = 232  (InstanceKlass 字段)
 *   _nonstatic_field_size = 288
 *   _static_field_size    = 292
 *   _init_state   = 394  (InstanceKlass 字段)
 */

#ifndef MY_JVM_OOPS_INSTANCEKLASS_HPP
#define MY_JVM_OOPS_INSTANCEKLASS_HPP

#include "klass.hpp"
#include "array.hpp"
#include "method.hpp"

// ========== 前向声明 ==========

class ConstantPool;
class Annotations;
class PackageEntry;
class OopMapCache;
class Thread;
class nmethod;
template <class T> class Array;

// ========== 类初始化状态 ==========
// 参考：instanceKlass.hpp 第 133-140 行

enum ClassState {
    allocated,                          // 已分配（但未链接）
    loaded,                             // 已加载并插入类层次
    linked,                             // 已成功链接/验证
    being_initialized,                  // 正在运行类初始化器
    fully_initialized,                  // 已完全初始化（成功最终状态）
    initialization_error                // 初始化过程中出错
};

// ========== OopMapBlock ==========
// 用于快速访问对象中的 oop（对象指针）字段
// 参考：instanceKlass.hpp 第 110-112 行

class OopMapBlock {
public:
    int  _offset;   // 起始偏移（从对象头开始）
    uint _count;    // 连续 oop 字段的数量（无符号）

    int  offset() const { return _offset; }
    void set_offset(int offset) { _offset = offset; }
    uint count() const { return _count; }
    void set_count(uint count) { _count = count; }
};

// ========== InstanceKlass 类 ==========

class InstanceKlass : public Klass {
protected:
    // ========== 字段（严格按 OpenJDK 源码顺序）==========
    // 参考：instanceKlass.hpp 第 155-310 行

    // 注解
    Annotations*    _annotations;

    // 包信息
    PackageEntry*   _package_entry;

    // 数组类（持有此类作为元素类型的数组类）
    Klass* volatile _array_klasses;

    // 常量池
    // GDB: offsetof(_constants) = 232
    ConstantPool*   _constants;

    // 内部类属性（InnerClasses + EnclosingMethod）
    Array<u2>*      _inner_classes;

    // NestMembers 属性
    Array<u2>*      _nest_members;

    // NestHost 索引
    u2              _nest_host_index;

    // 解析后的 nest-host klass
    InstanceKlass*  _nest_host;

    // source debug extension
    const char*     _source_debug_extension;

    // 数组名（从此类派生的数组类名）
    // 注意：Symbol* 是 8 字节指针
    void*           _array_name;   // Symbol* 简化为 void*

    // 非静态字段大小（单位：heapOopSize word，含继承字段）
    // GDB: offsetof(_nonstatic_field_size) = 288
    int             _nonstatic_field_size;

    // 静态字段大小（单位：word，含 oop 和非 oop）
    // GDB: offsetof(_static_field_size) = 292
    int             _static_field_size;

    // 泛型签名索引
    u2              _generic_signature_index;

    // 源文件名索引
    u2              _source_file_name_index;

    // 静态 oop 字段数量
    u2              _static_oop_field_count;

    // Java 声明字段数量
    u2              _java_fields_count;

    // 非静态 oop map 块大小（单位：word）
    int             _nonstatic_oop_map_size;

    // Java itable 长度（单位：word）
    // GDB: offsetof(_itable_len) = 308
    int             _itable_len;

    // 标记为依赖（用于 flush 和 deopt）
    bool            _is_marked_dependent;

    // 正在被重定义
    bool            _is_being_redefined;

    // 杂项标志
    u2              _misc_flags;

    // 类文件版本号
    u2              _minor_version;
    u2              _major_version;

    // 当前正在执行初始化的线程
    Thread*         _init_thread;

    // OopMap 缓存（懒加载）
    OopMapCache* volatile _oop_map_cache;

    // JNI id（静态字段）
    void*           _jni_ids;       // JNIid* 简化为 void*

    // jmethodID 数组
    void* volatile  _methods_jmethod_ids;

    // 依赖上下文
    intptr_t        _dep_context;

    // OSR nmethod 链表头
    nmethod*        _osr_nmethods_head;

    // JVMTI 断点列表
    void*           _breakpoints;   // BreakpointInfo* 简化为 void*

    // 前一个版本（用于 RedefineClasses）
    InstanceKlass*  _previous_versions;

    // JVMTI 缓存的类文件
    void*           _cached_class_file;

    // JNI/JVMTI 方法 ID 分配计数
    volatile u2     _idnum_allocated_count;

    // 类初始化状态
    // GDB: offsetof(_init_state) = 394
    u1              _init_state;

    // 引用类型
    u1              _reference_type;

    // 常量池中此类的索引
    u2              _this_class_index;

    // JVMTI 缓存的类字段 map
    void*           _jvmti_cached_class_field_map;

    // NOT_PRODUCT: 验证计数
    int             _verify_count;

    // 方法数组
    // GDB: offsetof(_methods) = 416
    Array<Method*>* _methods;

    // 默认方法数组（从接口继承的具体方法）
    Array<Method*>* _default_methods;

    // 本地接口数组
    Array<Klass*>*  _local_interfaces;

    // 传递接口数组
    Array<Klass*>*  _transitive_interfaces;

    // 方法原始顺序（用于 JVMTI）
    Array<int>*     _method_ordering;

    // 默认方法的 vtable 索引
    Array<int>*     _default_vtable_indices;

    // 字段信息数组
    Array<u2>*      _fields;

public:
    // ========== 构造函数 ==========

    InstanceKlass() :
        _annotations(nullptr), _package_entry(nullptr),
        _array_klasses(nullptr), _constants(nullptr),
        _inner_classes(nullptr), _nest_members(nullptr),
        _nest_host_index(0), _nest_host(nullptr),
        _source_debug_extension(nullptr), _array_name(nullptr),
        _nonstatic_field_size(0), _static_field_size(0),
        _generic_signature_index(0), _source_file_name_index(0),
        _static_oop_field_count(0), _java_fields_count(0),
        _nonstatic_oop_map_size(0), _itable_len(0),
        _is_marked_dependent(false), _is_being_redefined(false),
        _misc_flags(0), _minor_version(0), _major_version(0),
        _init_thread(nullptr), _oop_map_cache(nullptr),
        _jni_ids(nullptr), _methods_jmethod_ids(nullptr),
        _dep_context(0), _osr_nmethods_head(nullptr),
        _breakpoints(nullptr), _previous_versions(nullptr),
        _cached_class_file(nullptr), _idnum_allocated_count(0),
        _init_state(allocated), _reference_type(0),
        _this_class_index(0), _jvmti_cached_class_field_map(nullptr),
        _verify_count(0),
        _methods(nullptr), _default_methods(nullptr),
        _local_interfaces(nullptr), _transitive_interfaces(nullptr),
        _method_ordering(nullptr), _default_vtable_indices(nullptr),
        _fields(nullptr) {}

    // ========== 类型判断 ==========

    bool is_instance_klass() const override { return true; }

    // ========== 初始化状态 ==========

    u1 init_state() const { return _init_state; }
    void set_init_state(u1 state) { _init_state = state; }

    bool is_loaded() const           { return _init_state >= loaded; }
    bool is_linked() const           { return _init_state >= linked; }
    bool is_initialized() const      { return _init_state == fully_initialized; }
    bool is_being_initialized() const{ return _init_state == being_initialized; }
    bool is_in_error_state() const   { return _init_state == initialization_error; }

    // ========== 方法相关 ==========

    Array<Method*>* methods() const { return _methods; }
    void set_methods(Array<Method*>* m) { _methods = m; }

    Array<Method*>* default_methods() const { return _default_methods; }
    void set_default_methods(Array<Method*>* m) { _default_methods = m; }

    // ========== 常量池 ==========

    ConstantPool* constants() const { return _constants; }
    void set_constants(ConstantPool* c) { _constants = c; }

    // ========== 字段相关 ==========

    int nonstatic_field_size() const { return _nonstatic_field_size; }
    void set_nonstatic_field_size(int size) { _nonstatic_field_size = size; }

    int static_field_size() const { return _static_field_size; }
    void set_static_field_size(int size) { _static_field_size = size; }

    // ========== 接口相关 ==========

    Array<Klass*>* local_interfaces() const { return _local_interfaces; }
    void set_local_interfaces(Array<Klass*>* a) { _local_interfaces = a; }

    Array<Klass*>* transitive_interfaces() const { return _transitive_interfaces; }
    void set_transitive_interfaces(Array<Klass*>* a) { _transitive_interfaces = a; }

    // ========== itable ==========

    int itable_length() const { return _itable_len; }
    void set_itable_length(int len) { _itable_len = len; }

    // ========== 版本号 ==========

    u2 minor_version() const { return _minor_version; }
    void set_minor_version(u2 v) { _minor_version = v; }

    u2 major_version() const { return _major_version; }
    void set_major_version(u2 v) { _major_version = v; }

    // ========== OopMap（内联存储，在结构体之后）==========
    // 参考：instanceKlass.hpp 第 490 行
    // OopMapBlock 内联在 InstanceKlass 结构体之后（vtable 之后）

    OopMapBlock* start_of_nonstatic_oop_maps() const {
        // vtable 在 InstanceKlass 之后，oop maps 在 vtable 之后
        // 简化版：直接返回 nullptr（完整实现需要计算 vtable 偏移）
        return nullptr;
    }

    int nonstatic_oop_map_size() const { return _nonstatic_oop_map_size; }
    void set_nonstatic_oop_map_size(int words) { _nonstatic_oop_map_size = words; }

    unsigned int nonstatic_oop_map_count() const {
        // OopMapBlock::size_in_words() = 1（8字节/8字节 = 1 word）
        return _nonstatic_oop_map_size;
    }
};


// ========== 类型别名 ==========

typedef InstanceKlass* InstanceKlassPtr;

#endif // MY_JVM_OOPS_INSTANCEKLASS_HPP
