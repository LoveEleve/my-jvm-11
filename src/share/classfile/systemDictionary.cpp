#include "classfile/systemDictionary.hpp"
#include "classfile/classLoader.hpp"
#include "oops/instanceKlass.hpp"
#include "runtime/javaThread.hpp"

#include <cstring>
#include <cstdio>
#include <cstdlib>

// ============================================================================
// SystemDictionary — 类字典
// 对照 HotSpot: src/hotspot/share/classfile/systemDictionary.cpp
// ============================================================================

SystemDictionary::DictionaryEntry* SystemDictionary::_entries = nullptr;
int SystemDictionary::_size = 0;

void SystemDictionary::initialize() {
    // 对照: SystemDictionary::initialize()
    // HotSpot 中在 Universe::genesis() 里调用
    // 创建 Dictionary 哈希表、设置 _java_system_loader 等
    _entries = nullptr;
    _size = 0;

    fprintf(stderr, "[VM] SystemDictionary::initialize: dictionary ready\n");
}

InstanceKlass* SystemDictionary::resolve_or_null(const char* class_name, JavaThread* thread) {
    // 对照: SystemDictionary::resolve_or_null() [systemDictionary.cpp:246]
    //
    // HotSpot 完整流程:
    //   1. 数组类型? → resolve_array_class_or_null()
    //   2. 去掉 L/; 描述符包装
    //   3. → resolve_instance_class_or_null()
    //        a. dictionary->find()           ← 快速路径
    //        b. Placeholder 检查 (并行加载) ← 我们不需要
    //        c. load_instance_class()         ← 实际加载
    //        d. update_dictionary()           ← 注册到缓存

    // Step 1: 查字典缓存
    InstanceKlass* k = find(class_name);
    if (k != nullptr) {
        return k;  // 已加载，直接返回
    }

    // Step 2: 从文件系统加载
    // 对照: load_instance_class() → ClassLoader::load_class()
    // [systemDictionary.cpp:1403-1405]
    k = ClassLoader::load_class(class_name);
    if (k == nullptr) {
        // ClassNotFoundException
        fprintf(stderr, "[VM] SystemDictionary: class not found: %s\n", class_name);
        return nullptr;
    }

    // Step 3: 链接
    // 对照: InstanceKlass::link_class_impl() [instanceKlass.cpp:711]
    // 简化版：直接标记为 linked
    // Phase 13 会完善为真正的 link_class()
    if (!k->is_linked()) {
        k->set_init_state(InstanceKlass::linked);
    }

    // Step 4: 注册到字典
    // 对照: update_dictionary() [systemDictionary.cpp:1713]
    add_to_dictionary(class_name, k);

    fprintf(stderr, "[VM] SystemDictionary: registered \"%s\" (%p)\n",
            class_name, (void*)k);

    return k;
}

InstanceKlass* SystemDictionary::find(const char* class_name) {
    // 对照: Dictionary::find()
    // 简单链表遍历
    DictionaryEntry* entry = _entries;
    while (entry != nullptr) {
        if (strcmp(entry->class_name, class_name) == 0) {
            return entry->klass;
        }
        entry = entry->next;
    }
    return nullptr;
}

void SystemDictionary::add_to_dictionary(const char* class_name, InstanceKlass* klass) {
    // 复制类名
    size_t len = strlen(class_name);
    char* name_copy = (char*)malloc(len + 1);
    strcpy(name_copy, class_name);

    // 头插法
    DictionaryEntry* entry = (DictionaryEntry*)malloc(sizeof(DictionaryEntry));
    entry->class_name = name_copy;
    entry->klass = klass;
    entry->next = _entries;
    _entries = entry;
    _size++;
}

void SystemDictionary::destroy() {
    DictionaryEntry* entry = _entries;
    while (entry != nullptr) {
        DictionaryEntry* next = entry->next;
        free((void*)entry->class_name);
        // 销毁已加载的 InstanceKlass 及其关联数据
        delete entry->klass;
        free(entry);
        entry = next;
    }
    _entries = nullptr;
    _size = 0;
}
