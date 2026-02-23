// ============================================================================
// Mini JVM - InstanceKlass 实现
// 对应 HotSpot: src/hotspot/share/oops/instanceKlass.cpp
// ============================================================================

#include "oops/instanceKlass.hpp"
#include "gc/shared/javaHeap.hpp"
#include <cstring>

// ============================================================================
// 构造/析构
// ============================================================================

InstanceKlass::InstanceKlass()
    : Klass(InstanceKlassID),
      _constants(nullptr),
      _methods_count(0),
      _methods(nullptr),
      _fields_count(0),
      _field_infos(nullptr),
      _local_interfaces_count(0),
      _local_interfaces(nullptr),
      _init_state(allocated),
      _nonstatic_field_size(0),
      _static_field_size(0),
      _nonstatic_oop_map_size(0),
      _java_fields_count(0),
      _itable_len(0),
      _misc_flags(0),
      _minor_version(0),
      _major_version(0),
      _source_file_name_index(0),
      _class_name(nullptr),
      _super_class_name(nullptr),
      _static_fields(nullptr),
      _static_fields_count(0)
{}

InstanceKlass::~InstanceKlass() {
    // 释放方法
    if (_methods != nullptr) {
        for (int i = 0; i < _methods_count; i++) {
            if (_methods[i] != nullptr) {
                delete _methods[i];
            }
        }
        FREE_C_HEAP_ARRAY(Method*, _methods);
    }

    // 释放字段信息
    if (_field_infos != nullptr) {
        FREE_C_HEAP_ARRAY(FieldInfoEntry, _field_infos);
    }

    // 释放接口数组
    if (_local_interfaces != nullptr) {
        FREE_C_HEAP_ARRAY(Klass*, _local_interfaces);
    }

    // 释放类名
    if (_class_name != nullptr) {
        FREE_C_HEAP_ARRAY(char, _class_name);
    }
    if (_super_class_name != nullptr) {
        FREE_C_HEAP_ARRAY(char, _super_class_name);
    }

    // 释放静态字段数组
    if (_static_fields != nullptr) {
        FREE_C_HEAP_ARRAY(intptr_t, _static_fields);
    }

    // 释放常量池（InstanceKlass 拥有所有权）
    if (_constants != nullptr) {
        delete _constants;
    }
}

// ============================================================================
// 类名设置
// ============================================================================

void InstanceKlass::set_class_name(const char* name) {
    if (_class_name != nullptr) {
        FREE_C_HEAP_ARRAY(char, _class_name);
    }
    if (name != nullptr) {
        int len = (int)strlen(name);
        _class_name = NEW_C_HEAP_ARRAY(char, len + 1, mtClass);
        memcpy(_class_name, name, len + 1);
    } else {
        _class_name = nullptr;
    }
    // 同时设置 Klass 基类的 _name
    Klass::set_name(_class_name);
}

void InstanceKlass::set_super_class_name(const char* name) {
    if (_super_class_name != nullptr) {
        FREE_C_HEAP_ARRAY(char, _super_class_name);
    }
    if (name != nullptr) {
        int len = (int)strlen(name);
        _super_class_name = NEW_C_HEAP_ARRAY(char, len + 1, mtClass);
        memcpy(_super_class_name, name, len + 1);
    } else {
        _super_class_name = nullptr;
    }
}

// ============================================================================
// 方法查找
// ============================================================================

Method* InstanceKlass::find_method(const char* name, const char* signature) const {
    if (_constants == nullptr) return nullptr;

    for (int i = 0; i < _methods_count; i++) {
        Method* m = _methods[i];
        if (m == nullptr) continue;

        const char* m_name = _constants->utf8_at(m->name_index());
        const char* m_sig  = _constants->utf8_at(m->signature_index());

        if (strcmp(m_name, name) == 0 && strcmp(m_sig, signature) == 0) {
            return m;
        }
    }
    return nullptr;
}

// ============================================================================
// 工厂方法：从解析器数据创建 InstanceKlass
// 这是 Phase 2 (ClassFileParser) 和 Phase 3 (OOP-Klass) 的衔接点
//
// HotSpot 对应：ClassFileParser::create_instance_klass()
//   → InstanceKlass::allocate_instance_klass()
// ============================================================================

InstanceKlass* InstanceKlass::create_from_parser(
    const char* class_name,
    const char* super_class_name,
    u2 access_flags,
    u2 major_version,
    u2 minor_version,
    ConstantPool* cp,
    FieldInfoEntry* fields, int fields_count,
    Method** methods, int methods_count)
{
    InstanceKlass* ik = new InstanceKlass();

    // 基本信息
    ik->set_class_name(class_name);
    ik->set_super_class_name(super_class_name);
    ik->set_access_flags(AccessFlags(access_flags));
    ik->set_modifier_flags(access_flags & JVM_ACC_WRITTEN_FLAGS);
    ik->set_major_version(major_version);
    ik->set_minor_version(minor_version);

    // 常量池
    ik->set_constants(cp);

    // 方法：转移所有权
    ik->set_methods(methods, methods_count);

    // 字段：转移所有权
    ik->set_fields(fields, fields_count);

    // 计算实例大小
    // 简化版：header + 所有非静态字段
    int instance_size = instanceOopDesc::base_offset_in_bytes();
    int nonstatic_count = 0;
    for (int i = 0; i < fields_count; i++) {
        if (fields[i].is_static()) {
            // static 字段不在实例中分配空间
            fields[i].offset = FieldInfoEntry::invalid_offset;
            continue;
        }
        nonstatic_count++;
        // 根据描述符判断字段大小并分配偏移
        fields[i].offset = (u2)instance_size;
        const char* desc = cp->utf8_at(fields[i].descriptor_index);
        int field_size = 4; // 默认 4 字节 (I/F)
        if (desc[0] == 'J' || desc[0] == 'D') {
            // long / double = 8 字节
            instance_size = (int)align_up((uintx)instance_size, (uintx)8);
            fields[i].offset = (u2)instance_size;
            field_size = 8;
        } else if (desc[0] == 'L' || desc[0] == '[') {
            // 引用类型 = oopSize = 8 字节 (LP64)
            instance_size = (int)align_up((uintx)instance_size, (uintx)oopSize);
            fields[i].offset = (u2)instance_size;
            field_size = oopSize;
        } else if (desc[0] == 'B' || desc[0] == 'Z') {
            // byte / boolean = 1 字节
            field_size = 1;
        } else if (desc[0] == 'S' || desc[0] == 'C') {
            // short / char = 2 字节
            instance_size = (int)align_up((uintx)instance_size, (uintx)2);
            fields[i].offset = (u2)instance_size;
            field_size = 2;
        }
        instance_size += field_size;
    }

    // 对齐到 HeapWord
    instance_size = (int)align_up((uintx)instance_size, (uintx)HeapWordSize);
    ik->set_instance_size(instance_size);
    ik->set_nonstatic_field_size(nonstatic_count);

    if (nonstatic_count > 0) {
        ik->set_has_nonstatic_fields();
    }

    // Phase 5: 初始化静态字段存储
    // 统计静态字段数量并分配存储数组
    int static_count = 0;
    for (int i = 0; i < fields_count; i++) {
        if (fields[i].is_static()) {
            static_count++;
        }
    }
    if (static_count > 0) {
        ik->_static_fields = NEW_C_HEAP_ARRAY(intptr_t, static_count, mtClass);
        memset(ik->_static_fields, 0, sizeof(intptr_t) * static_count);
        ik->_static_fields_count = static_count;

        // 为每个静态字段分配 static_fields 数组中的索引
        // 复用 FieldInfoEntry::offset 字段来存储索引
        int static_idx = 0;
        for (int i = 0; i < fields_count; i++) {
            if (fields[i].is_static()) {
                fields[i].offset = (u2)static_idx;
                static_idx++;
            }
        }
    }

    // 设置状态为 loaded
    ik->set_init_state(loaded);

    return ik;
}

// ============================================================================
// 调试打印
// ============================================================================

void InstanceKlass::print_on(FILE* out) const {
    fprintf(out, "InstanceKlass(%p): \"%s\"", (void*)this, _class_name ? _class_name : "<null>");
    fprintf(out, " super=\"%s\"", _super_class_name ? _super_class_name : "<null>");
    fprintf(out, " state=%d", (int)_init_state);
    fprintf(out, " instance_size=%d", layout_helper());
    fprintf(out, " fields=%d methods=%d", _fields_count, _methods_count);
}

void InstanceKlass::print_summary(FILE* out) const {
    fprintf(out, "=== InstanceKlass Summary ===\n");
    fprintf(out, "  Class:    %s\n", _class_name ? _class_name : "<null>");
    fprintf(out, "  Super:    %s\n", _super_class_name ? _super_class_name : "<null>");
    fprintf(out, "  Version:  %d.%d (Java %d)\n",
            _major_version, _minor_version, _major_version - 44);
    fprintf(out, "  Flags:    ");
    _access_flags.print_on(out);
    fprintf(out, "\n");
    fprintf(out, "  State:    %d (%s)\n", (int)_init_state,
            _init_state == allocated ? "allocated" :
            _init_state == loaded ? "loaded" :
            _init_state == linked ? "linked" :
            _init_state == fully_initialized ? "initialized" : "other");
    fprintf(out, "  Instance size: %d bytes (%d HeapWords)\n",
            layout_helper(), layout_helper() / HeapWordSize);

    // 字段
    fprintf(out, "  Fields: %d\n", _fields_count);
    if (_constants != nullptr) {
        for (int i = 0; i < _fields_count; i++) {
            const FieldInfoEntry& f = _field_infos[i];
            if (f.is_static()) {
                fprintf(out, "    [%d] %s %s (flags=0x%04X, static)\n",
                        i,
                        _constants->utf8_at(f.descriptor_index),
                        _constants->utf8_at(f.name_index),
                        f.access_flags);
            } else {
                fprintf(out, "    [%d] %s %s (flags=0x%04X, offset=%d)\n",
                        i,
                        _constants->utf8_at(f.descriptor_index),
                        _constants->utf8_at(f.name_index),
                        f.access_flags,
                        f.offset);
            }
        }
    }

    // 方法
    fprintf(out, "  Methods: %d\n", _methods_count);
    if (_constants != nullptr) {
        for (int i = 0; i < _methods_count; i++) {
            Method* m = _methods[i];
            if (m == nullptr) continue;
            fprintf(out, "    [%d] %s%s (flags=0x%04X, code_size=%d, max_stack=%d, max_locals=%d)\n",
                    i,
                    _constants->utf8_at(m->name_index()),
                    _constants->utf8_at(m->signature_index()),
                    m->access_flags().as_int(),
                    m->code_size(),
                    m->max_stack(),
                    m->max_locals());
        }
    }
    fprintf(out, "\n");
}

// ============================================================================
// Phase 5: 对象分配
//
// 对应 HotSpot: InstanceKlass::allocate_instance()
//   → CollectedHeap::obj_allocate(klass, size)
//     → MemAllocator::allocate()
//       → mem_allocate() + finish()
//
// 流程：
//   1. 获取 instance_size()
//   2. 从 JavaHeap 分配
//   3. 返回初始化好的 oopDesc*
// ============================================================================

oopDesc* InstanceKlass::allocate_instance() {
    int size = instance_size();
    guarantee(size > 0, "instance_size must be positive");

    JavaHeap* heap = JavaHeap::heap();
    guarantee(heap != nullptr, "JavaHeap not initialized");

    oopDesc* obj = heap->obj_allocate(this, size);
    if (obj == nullptr) {
        fprintf(stderr, "ERROR: Failed to allocate instance of %s (%d bytes)\n",
                _class_name ? _class_name : "<null>", size);
        return nullptr;
    }

    return obj;
}

// ============================================================================
// Phase 5: 静态字段访问
// ============================================================================

intptr_t InstanceKlass::static_field_value(int field_index) const {
    vm_assert(field_index >= 0 && field_index < _static_fields_count,
              "static field index out of bounds");
    return _static_fields[field_index];
}

void InstanceKlass::set_static_field_value(int field_index, intptr_t value) {
    vm_assert(field_index >= 0 && field_index < _static_fields_count,
              "static field index out of bounds");
    _static_fields[field_index] = value;
}

const FieldInfoEntry* InstanceKlass::find_field(const char* name) const {
    if (_constants == nullptr || _field_infos == nullptr) return nullptr;

    for (int i = 0; i < _fields_count; i++) {
        const char* field_name = _constants->utf8_at(_field_infos[i].name_index);
        if (strcmp(field_name, name) == 0) {
            return &_field_infos[i];
        }
    }
    return nullptr;
}

int InstanceKlass::static_field_index(const char* name) const {
    const FieldInfoEntry* entry = find_field(name);
    if (entry != nullptr && entry->is_static()) {
        return (int)entry->offset;  // offset 存储的是 static_fields 数组的索引
    }
    return -1;
}
