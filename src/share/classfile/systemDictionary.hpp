#ifndef SHARE_CLASSFILE_SYSTEMDICTIONARY_HPP
#define SHARE_CLASSFILE_SYSTEMDICTIONARY_HPP

// ============================================================================
// Mini JVM - SystemDictionary（类字典 / 已加载类的缓存）
// 对照 HotSpot: src/hotspot/share/classfile/systemDictionary.hpp
//
// SystemDictionary 是类加载的核心调度中心：
//   - 维护 "类名 → InstanceKlass*" 的哈希表
//   - 先查缓存，未命中才触发实际加载
//   - 加载完成后注册到字典
//
// HotSpot 的 SystemDictionary:
//   - 使用 Dictionary（基于 Hashtable<Klass*, mtClass>）
//   - 支持并行类加载（Placeholder 表 + 双重检查）
//   - 支持多类加载器（每个加载器有独立的 Dictionary）
//
// 我们简化为:
//   - 单一的全局字典（不区分类加载器）
//   - 用简单的链表/数组实现（避免引入 STL）
//   - 不支持并行加载（单线程）
//
// 核心流程:
//   resolve_or_null("HelloWorld")
//     → find("HelloWorld")          先查缓存
//     → ClassLoader::load_class()   缓存未命中，从文件加载
//     → link_class()                链接
//     → add_to_dictionary()         注册到缓存
//     → return InstanceKlass*
// ============================================================================

#include "utilities/globalDefinitions.hpp"

class InstanceKlass;
class JavaThread;

class SystemDictionary {
public:
    // 初始化
    // 对照: SystemDictionary::initialize()
    // 在 Universe::genesis() 中调用
    static void initialize();

    // ★ 核心方法：解析类名 → InstanceKlass*
    // 对照: SystemDictionary::resolve_or_null() [systemDictionary.cpp:246]
    //
    // 完整流程:
    //   1. 查字典缓存 → 命中则返回
    //   2. ClassLoader::load_class() 从文件加载
    //   3. link_class() 链接
    //   4. 注册到字典
    //   5. 返回 InstanceKlass*
    //
    // 失败返回 nullptr（相当于 ClassNotFoundException）
    static InstanceKlass* resolve_or_null(const char* class_name, JavaThread* thread);

    // 查找已加载的类（不触发加载）
    // 对照: Dictionary::find()
    static InstanceKlass* find(const char* class_name);

    // 清理
    static void destroy();

private:
    // 简单的字典实现：链表
    // HotSpot 用 Dictionary（Hashtable 子类），我们简化为链表
    struct DictionaryEntry {
        const char*     class_name;  // 类名（堆分配的副本）
        InstanceKlass*  klass;
        DictionaryEntry* next;
    };

    static DictionaryEntry* _entries;
    static int              _size;

    // 注册到字典
    // 对照: SystemDictionary::update_dictionary() [systemDictionary.cpp:1713]
    static void add_to_dictionary(const char* class_name, InstanceKlass* klass);
};

#endif // SHARE_CLASSFILE_SYSTEMDICTIONARY_HPP
