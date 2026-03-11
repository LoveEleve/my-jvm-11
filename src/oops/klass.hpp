/*
 * my_jvm - Klass (Class Metadata) definitions
 * 
 * 类元数据的基类，来自 OpenJDK 11 hotspot/src/share/vm/oops/klass.hpp
 * 对标 OpenJDK 11 slowdebug 版本
 * 
 * 内存布局（64-bit slowdebug）：
 *   [0]   Metadata (vtable + _valid + padding) = 16 bytes
 *   [16]  _layout_helper: jint (4)
 *   [20]  _id: KlassID (4)
 *   [24]  _super_check_offset: juint (4) + padding (4)
 *   [28]  _name: Symbol* (8)  ← 实际 offset=24 见 GDB
 *   ...
 *   sizeof(Klass) = 208
 */

#ifndef MY_JVM_OOPS_KLASS_HPP
#define MY_JVM_OOPS_KLASS_HPP

#include "globalDefinitions.hpp"
#include "metadata.hpp"

// ========== 前向声明 ==========

class Symbol;
class ClassLoaderData;
template <class T> class Array;

// ========== KlassID 枚举 ==========
// 参考：klass.hpp 第 44-51 行

enum KlassID {
    InstanceKlassID,
    InstanceRefKlassID,
    InstanceMirrorKlassID,
    InstanceClassLoaderKlassID,
    TypeArrayKlassID,
    ObjArrayKlassID
};

// ========== OopHandle（简化版）==========
// 在 OpenJDK 中是 oop* 的包装，这里简化为 void*

typedef void* OopHandle;

// ========== Klass 类 ==========
// 类元数据的基类，存在 Metaspace 中
//
// 内存布局（64-bit slowdebug）：
//   [0]   Metadata base (16 bytes: vtable + _valid + padding)
//   [16]  _layout_helper: jint (4)
//   [20]  _id: KlassID (4)
//   [24]  _super_check_offset: juint (4)
//   [28]  padding (4)
//   [32]  _name: Symbol* (8)  ← GDB: offsetof(_name)=24 ???
//
// 注意：GDB 显示 offsetof(_name)=24，offsetof(_layout_helper)=12
// 这说明 Metadata 基类 sizeof=16，但 _layout_helper 在 offset 12
// 即 Metadata 内部布局：vtable(8) + _valid(4) = 12，_layout_helper 紧跟
// 实际上 _layout_helper 是 Klass 的第一个字段，offset = sizeof(Metadata) - padding
// GDB 实测：_layout_helper=12, _name=24, _super=120, _access_flags=164
//
// 字段顺序严格按照 OpenJDK klass.hpp 源码

class Klass : public Metadata {
protected:
    enum { _primary_super_limit = 8 };

    // ========== 高频字段（放在最前面，提升缓存命中率）==========
    
    // 布局辅助字段：描述对象内存布局
    // 实例类：正数=实例大小（字节，已对齐）
    // 数组类：负数，包含 tag/hsz/ebt/log2(esz)
    // 其他类：0
    // 参考：klass.hpp 第 115 行
    jint        _layout_helper;

    // Klass 标识符，用于实现去虚化的 oop closure 分发
    const KlassID _id;

    // 快速子类型检查相关字段
    // 参考：klass.hpp 第 125-135 行
    juint       _super_check_offset;    // 观察超类型的位置

    // 类名（Symbol*）
    // 实例类：java/lang/String 等
    // 数组类：[I, [Ljava/lang/String; 等
    Symbol*     _name;

    // 快速子类型检查缓存
    Klass*      _secondary_super_cache; // 最近观察到的次级超类型缓存
    Array<Klass*>* _secondary_supers;  // 所有次级超类型数组
    Klass*      _primary_supers[_primary_super_limit]; // 主超类型有序列表（8个）

    // java/lang/Class 镜像（对应此 Klass 的 Java 层 Class 对象）
    OopHandle   _java_mirror;

    // 超类
    Klass*      _super;

    // 第一个子类（NULL 表示无子类）
    Klass*      _subklass;

    // 兄弟链接（同一父类的所有子类链表）
    Klass*      _next_sibling;

    // 同一类加载器加载的所有 Klass 链表
    Klass*      _next_link;

    // 类加载器数据
    ClassLoaderData* _class_loader_data;

    // 修饰符标志（供 Class.getModifiers() 使用）
    jint        _modifier_flags;

    // 访问标志（class/interface 区别存在这里）
    // 注意：OpenJDK 中是 AccessFlags 类型（内部是 jint），这里简化为 juint
    juint       _access_flags;

    // JFR trace id（简化版省略，但需要占位以保持布局）
    // JFR_ONLY(DEFINE_TRACE_ID_FIELD;) → 在 slowdebug 中占 8 bytes
    // GDB 实测：_access_flags=164，sizeof(Klass)=208
    // 164 + 4(access_flags) + 8(jfr) + 8(last_biased) + 8(prototype) + 4(revocation) + 4(vtable_len) + 2(shared_path) + 2(shared_flags) = 208
    // 所以 JFR 字段占 8 bytes
    uint64_t    _jfr_trace_id;          // JFR trace id（简化为 uint64_t）

    // 偏向锁相关字段
    jlong       _last_biased_lock_bulk_revocation_time; // 最后一次批量撤销偏向锁的时间
    markOop     _prototype_header;      // 偏向锁原型头（启用/禁用偏向锁时使用）
    jint        _biased_lock_revocation_count; // 偏向锁撤销计数

    // vtable 长度
    int         _vtable_len;

private:
    // CDS 相关字段
    jshort      _shared_class_path_index; // 共享类路径索引（-1 表示非共享）
    u2          _shared_class_flags;      // 共享类标志

public:
    // ========== 构造函数 ==========
    
    Klass() : _layout_helper(0), _id(KlassID(-1)),
              _super_check_offset(0), _name(nullptr),
              _secondary_super_cache(nullptr), _secondary_supers(nullptr),
              _java_mirror(nullptr), _super(nullptr),
              _subklass(nullptr), _next_sibling(nullptr),
              _next_link(nullptr), _class_loader_data(nullptr),
              _modifier_flags(0), _access_flags(0),
              _jfr_trace_id(0),
              _last_biased_lock_bulk_revocation_time(0),
              _prototype_header(nullptr),
              _biased_lock_revocation_count(0),
              _vtable_len(0),
              _shared_class_path_index(-1),
              _shared_class_flags(0) {
        for (int i = 0; i < _primary_super_limit; i++) {
            _primary_supers[i] = nullptr;
        }
    }
    
    // ========== 布局辅助 ==========
    
    jint layout_helper() const { return _layout_helper; }
    void set_layout_helper(jint lh) { _layout_helper = lh; }
    
    bool is_array() const { return _layout_helper < 0; }
    
    // ========== 修饰符和访问标志 ==========
    
    jint modifier_flags() const { return _modifier_flags; }
    juint access_flags() const { return _access_flags; }
    
    void set_modifier_flags(jint flags) { _modifier_flags = flags; }
    void set_access_flags(juint flags) { _access_flags = flags; }
    
    // ========== 类层次关系 ==========
    
    Klass* super() const { return _super; }
    void set_super(Klass* k) { _super = k; }
    
    Klass* subklass() const { return _subklass; }
    void set_subklass(Klass* k) { _subklass = k; }
    
    Klass* next_sibling() const { return _next_sibling; }
    void set_next_sibling(Klass* k) { _next_sibling = k; }
    
    Klass* next_link() const { return _next_link; }
    void set_next_link(Klass* k) { _next_link = k; }
    
    // ========== 名称 ==========
    
    Symbol* name() const { return _name; }
    void set_name(Symbol* n) { _name = n; }
    
    // ========== 超类型检查 ==========
    
    juint super_check_offset() const { return _super_check_offset; }
    void set_super_check_offset(juint o) { _super_check_offset = o; }
    
    Klass* secondary_super_cache() const { return _secondary_super_cache; }
    void set_secondary_super_cache(Klass* k) { _secondary_super_cache = k; }
    
    Array<Klass*>* secondary_supers() const { return _secondary_supers; }
    void set_secondary_supers(Array<Klass*>* k) { _secondary_supers = k; }
    
    // ========== vtable ==========
    
    int vtable_length() const { return _vtable_len; }
    void set_vtable_length(int len) { _vtable_len = len; }
    
    // ========== 类加载器 ==========
    
    ClassLoaderData* class_loader_data() const { return _class_loader_data; }
    void set_class_loader_data(ClassLoaderData* d) { _class_loader_data = d; }
    
    // ========== 类型判断 ==========
    
    bool is_klass() const override { return true; }
    
    virtual bool is_instance_klass() const { return false; }
    virtual bool is_array_klass() const { return false; }
    virtual bool is_objArray_klass() const { return false; }
    virtual bool is_typeArray_klass() const { return false; }
};

// ========== 类型别名 ==========

typedef Klass* KlassPtr;

#endif // MY_JVM_OOPS_KLASS_HPP
