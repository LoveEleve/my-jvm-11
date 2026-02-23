// ============================================================================
// Mini JVM - Class 文件解析器实现
// 对应 HotSpot: src/hotspot/share/classfile/classFileParser.cpp (6400+ 行)
//
// HotSpot 的 parse_stream() 方法（约第 6071-6316 行）是总入口，
// 严格按照 JVM 规范的 ClassFile 结构顺序解析。
// 我们的实现保持相同的顺序和结构。
// ============================================================================

#include "classfile/classFileParser.hpp"
#include "oops/instanceKlass.hpp"
#include <cstring>

// Java Class 文件魔数
static const u4 JAVA_MAGIC = 0xCAFEBABE;

// ============================================================================
// 构造/析构
// ============================================================================

ClassFileParser::ClassFileParser(const ClassFileStream* stream)
    : _stream(stream),
      _magic(0),
      _minor_version(0),
      _major_version(0),
      _cp(nullptr),
      _access_flags(0),
      _this_class_index(0),
      _super_class_index(0),
      _class_name(nullptr),
      _super_class_name(nullptr),
      _interfaces_count(0),
      _interfaces(nullptr),
      _fields_count(0),
      _fields(nullptr),
      _methods_count(0),
      _methods(nullptr) {
}

ClassFileParser::~ClassFileParser() {
    // 释放常量池
    if (_cp != nullptr) {
        delete _cp;
    }
    // 释放接口数组
    if (_interfaces != nullptr) {
        FREE_C_HEAP_ARRAY(u2, _interfaces);
    }
    // 释放字段数组
    if (_fields != nullptr) {
        delete[] _fields;
    }
    // 释放方法数组（MethodInfo 的析构函数会释放 code）
    if (_methods != nullptr) {
        delete[] _methods;
    }
}

// ============================================================================
// parse() — 主入口
// 对应 HotSpot: ClassFileParser::parse_stream()
//
// 严格按照 JVM 规范 ClassFile 结构的顺序：
//   ClassFile {
//       u4             magic;
//       u2             minor_version;
//       u2             major_version;
//       u2             constant_pool_count;
//       cp_info        constant_pool[constant_pool_count-1];
//       u2             access_flags;
//       u2             this_class;
//       u2             super_class;
//       u2             interfaces_count;
//       u2             interfaces[interfaces_count];
//       u2             fields_count;
//       field_info     fields[fields_count];
//       u2             methods_count;
//       method_info    methods[methods_count];
//       u2             attributes_count;
//       attribute_info attributes[attributes_count];
//   }
// ============================================================================

void ClassFileParser::parse() {
    // 步骤 1-2: 魔数和版本号
    parse_magic_and_version();

    // 步骤 3: 常量池
    parse_constant_pool();

    // 步骤 4: 访问标志
    parse_access_flags();

    // 步骤 5: this_class
    parse_this_class();

    // 步骤 6: super_class
    parse_super_class();

    // 步骤 7: 接口
    parse_interfaces();

    // 步骤 8: 字段
    parse_fields();

    // 步骤 9: 方法
    parse_methods();

    // 步骤 10: 类属性
    parse_class_attributes();

    // 验证流已读完
    // HotSpot: guarantee(stream->at_eos(), ...)
    // 我们允许有剩余字节（某些编译器会追加额外数据）
}

// ============================================================================
// 步骤 1-2: Magic + Version
// HotSpot: parse_stream() 第 6078-6102 行
// ============================================================================

void ClassFileParser::parse_magic_and_version() {
    // 读取魔数
    _magic = _stream->get_u4();
    guarantee(_magic == JAVA_MAGIC,
        "Invalid class file: bad magic number (not 0xCAFEBABE)");

    // 读取版本号
    _minor_version = _stream->get_u2();
    _major_version = _stream->get_u2();

    // HotSpot 会调用 verify_class_version() 检查版本合法性
    // Java 11 对应 major_version = 55
    // 我们只做基本检查
    guarantee(_major_version >= 45 && _major_version <= 55,
        "Unsupported class file version (need 45-55 for Java 1.1 ~ 11)");
}

// ============================================================================
// 步骤 3: 常量池解析
// HotSpot: parse_stream() 第 6104-6123 行
//          → parse_constant_pool()
//            → parse_constant_pool_entries()  (第一遍: 读取原始数据)
//            → post_process_constant_pool()   (第二遍: 验证+修正标签)
// ============================================================================

void ClassFileParser::parse_constant_pool() {
    // 读取常量池大小
    u2 cp_size = _stream->get_u2();
    guarantee(cp_size >= 1, "Invalid constant pool size");

    // 创建常量池对象
    _cp = new ConstantPool(cp_size);

    // 第一遍：从字节流读取所有条目
    parse_constant_pool_entries(cp_size);

    // 第二遍：验证交叉引用 + 修正临时标签
    post_process_constant_pool();
}

// ----------------------------------------------------------------------------
// 第一遍：读取常量池条目
// HotSpot: ClassFileParser::parse_constant_pool_entries() 第 127-376 行
//
// 核心是一个大 switch-case，根据 tag 读取不同数量的字节。
// 注意：常量池索引从 1 开始（第 0 个不用），Long/Double 占 2 个 slot。
// ----------------------------------------------------------------------------

void ClassFileParser::parse_constant_pool_entries(int length) {
    const ClassFileStream* cfs = _stream;

    for (int index = 1; index < length; index++) {
        u1 tag = cfs->get_u1();

        switch (tag) {

            case JVM_CONSTANT_Class: {
                // CONSTANT_Class_info { u1 tag; u2 name_index; }
                // name_index 指向一个 Utf8 条目，存储类的全限定名
                u2 name_index = cfs->get_u2();
                _cp->klass_index_at_put(index, name_index);
                break;
            }

            case JVM_CONSTANT_Fieldref: {
                // CONSTANT_Fieldref_info { u1 tag; u2 class_index; u2 name_and_type_index; }
                u2 class_index = cfs->get_u2();
                u2 name_and_type_index = cfs->get_u2();
                _cp->field_at_put(index, class_index, name_and_type_index);
                break;
            }

            case JVM_CONSTANT_Methodref: {
                // CONSTANT_Methodref_info { u1 tag; u2 class_index; u2 name_and_type_index; }
                u2 class_index = cfs->get_u2();
                u2 name_and_type_index = cfs->get_u2();
                _cp->method_at_put(index, class_index, name_and_type_index);
                break;
            }

            case JVM_CONSTANT_InterfaceMethodref: {
                // CONSTANT_InterfaceMethodref_info { u1 tag; u2 class_index; u2 name_and_type_index; }
                u2 class_index = cfs->get_u2();
                u2 name_and_type_index = cfs->get_u2();
                _cp->interface_method_at_put(index, class_index, name_and_type_index);
                break;
            }

            case JVM_CONSTANT_String: {
                // CONSTANT_String_info { u1 tag; u2 string_index; }
                // string_index 指向 Utf8 条目
                u2 string_index = cfs->get_u2();
                _cp->string_index_at_put(index, string_index);
                break;
            }

            case JVM_CONSTANT_Integer: {
                // CONSTANT_Integer_info { u1 tag; u4 bytes; }
                u4 bytes = cfs->get_u4();
                _cp->int_at_put(index, (jint)bytes);
                break;
            }

            case JVM_CONSTANT_Float: {
                // CONSTANT_Float_info { u1 tag; u4 bytes; }
                u4 bytes = cfs->get_u4();
                jfloat value;
                memcpy(&value, &bytes, sizeof(jfloat));
                _cp->float_at_put(index, value);
                break;
            }

            case JVM_CONSTANT_Long: {
                // CONSTANT_Long_info { u1 tag; u4 high_bytes; u4 low_bytes; }
                // 占 2 个 slot！index 会多跳一格
                u8 bytes = cfs->get_u8();
                _cp->long_at_put(index, (jlong)bytes);
                index++;  // Long 占 2 个 slot，跳过下一个
                break;
            }

            case JVM_CONSTANT_Double: {
                // CONSTANT_Double_info { u1 tag; u4 high_bytes; u4 low_bytes; }
                // 占 2 个 slot
                u8 bytes = cfs->get_u8();
                jdouble value;
                memcpy(&value, &bytes, sizeof(jdouble));
                _cp->double_at_put(index, value);
                index++;  // Double 占 2 个 slot
                break;
            }

            case JVM_CONSTANT_NameAndType: {
                // CONSTANT_NameAndType_info { u1 tag; u2 name_index; u2 descriptor_index; }
                u2 name_index = cfs->get_u2();
                u2 descriptor_index = cfs->get_u2();
                _cp->name_and_type_at_put(index, name_index, descriptor_index);
                break;
            }

            case JVM_CONSTANT_Utf8: {
                // CONSTANT_Utf8_info { u1 tag; u2 length; u1 bytes[length]; }
                u2 utf8_length = cfs->get_u2();
                const u1* utf8_buffer = cfs->get_u1_buffer();
                cfs->skip_u1(utf8_length);
                _cp->utf8_at_put(index, utf8_buffer, utf8_length);
                break;
            }

            case JVM_CONSTANT_MethodHandle: {
                // CONSTANT_MethodHandle_info { u1 tag; u1 reference_kind; u2 reference_index; }
                u1 ref_kind = cfs->get_u1();
                u2 method_index = cfs->get_u2();
                _cp->method_handle_index_at_put(index, ref_kind, method_index);
                break;
            }

            case JVM_CONSTANT_MethodType: {
                // CONSTANT_MethodType_info { u1 tag; u2 descriptor_index; }
                u2 signature_index = cfs->get_u2();
                _cp->method_type_index_at_put(index, signature_index);
                break;
            }

            case JVM_CONSTANT_InvokeDynamic: {
                // CONSTANT_InvokeDynamic_info { u1 tag; u2 bsm_attr_index; u2 name_and_type_index; }
                u2 bsm_index = cfs->get_u2();
                u2 name_and_type_index = cfs->get_u2();
                _cp->invoke_dynamic_at_put(index, bsm_index, name_and_type_index);
                break;
            }

            case JVM_CONSTANT_Dynamic: {
                // CONSTANT_Dynamic_info { u1 tag; u2 bsm_attr_index; u2 name_and_type_index; }
                u2 bsm_index = cfs->get_u2();
                u2 name_and_type_index = cfs->get_u2();
                _cp->dynamic_constant_at_put(index, bsm_index, name_and_type_index);
                break;
            }

            default: {
                fprintf(stderr, "ClassFormatError: Unknown constant pool tag %d at index %d\n",
                        tag, index);
                fatal("Unknown constant pool tag");
            }
        }
    }
}

// ----------------------------------------------------------------------------
// 第二遍：后处理常量池
// HotSpot: parse_constant_pool() 第 407-595 行
//
// 主要工作：
//   1. ClassIndex(101) → UnresolvedClass(100)
//   2. StringIndex(102) → String(8)
//   3. 验证各种引用的合法性
// ----------------------------------------------------------------------------

void ClassFileParser::post_process_constant_pool() {
    int length = _cp->length();

    for (int index = 1; index < length; index++) {
        constantTag tag = _cp->tag_at(index);

        switch (tag.value()) {

            case JVM_CONSTANT_ClassIndex: {
                // 验证 name_index 指向 Utf8
                int name_index = _cp->klass_name_index_at(index);
                guarantee(name_index > 0 && name_index < length,
                    "Bad Class name_index in constant pool");
                guarantee(_cp->tag_at(name_index).is_utf8(),
                    "Class name_index must point to Utf8");
                // 升级标签: ClassIndex → UnresolvedClass
                _cp->unresolved_klass_at_put(index, name_index);
                break;
            }

            case JVM_CONSTANT_StringIndex: {
                // 验证 utf8_index 指向 Utf8
                int utf8_index = _cp->string_utf8_index_at(index);
                guarantee(utf8_index > 0 && utf8_index < length,
                    "Bad String utf8_index in constant pool");
                guarantee(_cp->tag_at(utf8_index).is_utf8(),
                    "String utf8_index must point to Utf8");
                // 升级标签: StringIndex → String
                _cp->unresolved_string_at_put(index, utf8_index);
                break;
            }

            case JVM_CONSTANT_Long:
            case JVM_CONSTANT_Double:
                // 占 2 个 slot，跳过
                index++;
                break;

            default:
                // 其他类型暂不做额外验证
                break;
        }
    }
}

// ============================================================================
// 步骤 4: 访问标志
// HotSpot: parse_stream() 第 6127-6153 行
// ============================================================================

void ClassFileParser::parse_access_flags() {
    _access_flags = _stream->get_u2();
}

// ============================================================================
// 步骤 5: this_class
// HotSpot: parse_stream() 第 6156-6177 行
// ============================================================================

void ClassFileParser::parse_this_class() {
    _this_class_index = _stream->get_u2();
    guarantee(_this_class_index > 0 && _this_class_index < _cp->length(),
        "Bad this_class index");
    guarantee(_cp->tag_at(_this_class_index).is_unresolved_klass(),
        "this_class must be a Class entry");
    _class_name = _cp->klass_name_at(_this_class_index);
}

// ============================================================================
// 步骤 6: super_class
// HotSpot: parse_stream() 第 6253-6258 行
// ============================================================================

void ClassFileParser::parse_super_class() {
    _super_class_index = _stream->get_u2();
    if (_super_class_index == 0) {
        // 只有 java.lang.Object 的 super_class 是 0
        _super_class_name = nullptr;
    } else {
        guarantee(_super_class_index < _cp->length(),
            "Bad super_class index");
        guarantee(_cp->tag_at(_super_class_index).is_unresolved_klass(),
            "super_class must be a Class entry");
        _super_class_name = _cp->klass_name_at(_super_class_index);
    }
}

// ============================================================================
// 步骤 7: 接口
// HotSpot: parse_stream() 第 6260-6266 行 → parse_interfaces()
// ============================================================================

void ClassFileParser::parse_interfaces() {
    _interfaces_count = _stream->get_u2();

    if (_interfaces_count > 0) {
        _interfaces = NEW_C_HEAP_ARRAY(u2, _interfaces_count, mtClass);
        for (int i = 0; i < _interfaces_count; i++) {
            u2 iface_index = _stream->get_u2();
            guarantee(iface_index > 0 && iface_index < _cp->length(),
                "Bad interface index");
            guarantee(_cp->tag_at(iface_index).is_unresolved_klass(),
                "Interface must be a Class entry");
            _interfaces[i] = iface_index;
        }
    }
}

// ============================================================================
// 步骤 8: 字段
// HotSpot: parse_stream() 第 6270-6278 行 → parse_fields()
//
// field_info {
//     u2 access_flags;
//     u2 name_index;
//     u2 descriptor_index;
//     u2 attributes_count;
//     attribute_info attributes[attributes_count];
// }
// ============================================================================

void ClassFileParser::parse_fields() {
    _fields_count = _stream->get_u2();

    if (_fields_count == 0) return;

    _fields = new FieldInfo[_fields_count];

    for (int i = 0; i < _fields_count; i++) {
        FieldInfo& f = _fields[i];
        f.access_flags = _stream->get_u2();
        f.name_index = _stream->get_u2();
        f.descriptor_index = _stream->get_u2();
        f.attributes_count = _stream->get_u2();
        f.constant_value_index = 0;

        // 解析字段属性
        for (int j = 0; j < f.attributes_count; j++) {
            u2 attr_name_index = _stream->get_u2();
            u4 attr_length = _stream->get_u4();
            const char* attr_name = _cp->utf8_at(attr_name_index);

            if (strcmp(attr_name, "ConstantValue") == 0) {
                // ConstantValue_attribute { u2 constantvalue_index; }
                guarantee(attr_length == 2, "Bad ConstantValue attribute length");
                f.constant_value_index = _stream->get_u2();
            } else {
                // 跳过其他属性（Synthetic, Deprecated, Signature, 注解等）
                _stream->skip_u1(attr_length);
            }
        }
    }
}

// ============================================================================
// 步骤 9: 方法
// HotSpot: parse_stream() 第 6282-6298 行 → parse_methods()
//
// method_info {
//     u2 access_flags;
//     u2 name_index;
//     u2 descriptor_index;
//     u2 attributes_count;
//     attribute_info attributes[attributes_count];
// }
// ============================================================================

void ClassFileParser::parse_methods() {
    _methods_count = _stream->get_u2();

    if (_methods_count == 0) return;

    _methods = new MethodInfo[_methods_count];

    for (int i = 0; i < _methods_count; i++) {
        MethodInfo& m = _methods[i];
        m.access_flags = _stream->get_u2();
        m.name_index = _stream->get_u2();
        m.descriptor_index = _stream->get_u2();
        m.attributes_count = _stream->get_u2();

        // 解析方法属性
        for (int j = 0; j < m.attributes_count; j++) {
            u2 attr_name_index = _stream->get_u2();
            u4 attr_length = _stream->get_u4();
            const char* attr_name = _cp->utf8_at(attr_name_index);

            if (strcmp(attr_name, "Code") == 0) {
                parse_method_code_attribute(&m);
            } else {
                // 跳过 Exceptions, Signature, 注解等
                _stream->skip_u1(attr_length);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Code 属性解析
// Code_attribute {
//     u2 max_stack;
//     u2 max_locals;
//     u4 code_length;
//     u1 code[code_length];
//     u2 exception_table_length;
//     { u2 start_pc; u2 end_pc; u2 handler_pc; u2 catch_type; }
//         exception_table[exception_table_length];
//     u2 attributes_count;
//     attribute_info attributes[attributes_count];  // LineNumberTable 等
// }
// ----------------------------------------------------------------------------

void ClassFileParser::parse_method_code_attribute(MethodInfo* method) {
    method->max_stack = _stream->get_u2();
    method->max_locals = _stream->get_u2();
    method->code_length = _stream->get_u4();

    // 拷贝字节码
    if (method->code_length > 0) {
        method->code = NEW_C_HEAP_ARRAY(u1, method->code_length, mtCode);
        memcpy(method->code, _stream->get_u1_buffer(), method->code_length);
        _stream->skip_u1(method->code_length);
    }

    // 异常表
    method->exception_table_length = _stream->get_u2();
    // 每个异常表条目 = 4 个 u2 = 8 字节
    _stream->skip_u1(method->exception_table_length * 8);

    // Code 属性内部的子属性（LineNumberTable, StackMapTable 等）
    u2 code_attrs_count = _stream->get_u2();
    for (int k = 0; k < code_attrs_count; k++) {
        _stream->skip_u1(2);  // attribute_name_index
        u4 sub_attr_length = _stream->get_u4();
        _stream->skip_u1(sub_attr_length);
    }
}

// ============================================================================
// 步骤 10: 类属性
// HotSpot: parse_stream() 第 6300-6308 行 → parse_classfile_attributes()
// ============================================================================

void ClassFileParser::parse_class_attributes() {
    u2 attrs_count = _stream->get_u2();

    for (int i = 0; i < attrs_count; i++) {
        u2 attr_name_index = _stream->get_u2();
        u4 attr_length = _stream->get_u4();

        // 暂时跳过所有类属性
        // 后续可以解析 SourceFile, InnerClasses, BootstrapMethods 等
        _stream->skip_u1(attr_length);
    }
}

// ============================================================================
// 调试打印
// ============================================================================

void ClassFileParser::print_summary(FILE* out) const {
    fprintf(out, "=== Class File Summary ===\n");
    fprintf(out, "  Version: %d.%d (Java %d)\n",
            _major_version, _minor_version, _major_version - 44);
    fprintf(out, "  Access:  0x%04X", _access_flags);
    if (_access_flags & JVM_ACC_PUBLIC)    fprintf(out, " public");
    if (_access_flags & JVM_ACC_FINAL)     fprintf(out, " final");
    if (_access_flags & JVM_ACC_SUPER)     fprintf(out, " super");
    if (_access_flags & JVM_ACC_INTERFACE) fprintf(out, " interface");
    if (_access_flags & JVM_ACC_ABSTRACT)  fprintf(out, " abstract");
    if (_access_flags & JVM_ACC_SYNTHETIC) fprintf(out, " synthetic");
    if (_access_flags & JVM_ACC_ENUM)      fprintf(out, " enum");
    fprintf(out, "\n");

    fprintf(out, "  Class:   %s\n", _class_name ? _class_name : "<null>");
    fprintf(out, "  Super:   %s\n", _super_class_name ? _super_class_name : "<null>");

    // 接口
    fprintf(out, "  Interfaces: %d\n", _interfaces_count);
    for (int i = 0; i < _interfaces_count; i++) {
        fprintf(out, "    [%d] %s\n", i, interface_name_at(i));
    }

    // 字段
    fprintf(out, "  Fields: %d\n", _fields_count);
    for (int i = 0; i < _fields_count; i++) {
        const FieldInfo& f = _fields[i];
        fprintf(out, "    [%d] %s %s (flags=0x%04X)\n",
                i, f.descriptor(_cp), f.name(_cp), f.access_flags);
    }

    // 方法
    fprintf(out, "  Methods: %d\n", _methods_count);
    for (int i = 0; i < _methods_count; i++) {
        const MethodInfo& m = _methods[i];
        fprintf(out, "    [%d] %s%s (flags=0x%04X",
                i, m.name(_cp), m.descriptor(_cp), m.access_flags);
        if (m.code_length > 0) {
            fprintf(out, ", max_stack=%d, max_locals=%d, code_length=%d",
                    m.max_stack, m.max_locals, m.code_length);
        }
        fprintf(out, ")\n");
    }

    fprintf(out, "\n");
}

// ============================================================================
// create_instance_klass() — 将解析结果转化为 InstanceKlass
// 对应 HotSpot: ClassFileParser::create_instance_klass()
//
// 这是 Phase 2 (ClassFileParser) 和 Phase 3 (OOP-Klass) 的核心衔接点。
// 把临时的 FieldInfo/MethodInfo 转化为正式的 FieldInfoEntry/Method* 对象。
//
// HotSpot 的流程：
//   1. ClassFileParser::parse_stream() → 解析 .class 到临时数据
//   2. ClassFileParser::create_instance_klass() → 分配 InstanceKlass
//   3. ClassFileParser::fill_instance_klass() → 填充所有字段
//   4. InstanceKlass 持有 ConstantPool、Method*[] 等的所有权
// ============================================================================

InstanceKlass* ClassFileParser::create_instance_klass() {
    guarantee(_cp != nullptr, "must parse() before create_instance_klass()");

    // 1. 转换字段：FieldInfo → FieldInfoEntry
    FieldInfoEntry* field_entries = nullptr;
    if (_fields_count > 0) {
        field_entries = NEW_C_HEAP_ARRAY(FieldInfoEntry, _fields_count, mtClass);
        for (int i = 0; i < _fields_count; i++) {
            field_entries[i].access_flags = _fields[i].access_flags;
            field_entries[i].name_index = _fields[i].name_index;
            field_entries[i].descriptor_index = _fields[i].descriptor_index;
            field_entries[i].offset = 0;  // 由 create_from_parser 计算
            field_entries[i].constant_value_index = _fields[i].constant_value_index;
        }
    }

    // 2. 转换方法：MethodInfo → Method*
    Method** method_ptrs = nullptr;
    if (_methods_count > 0) {
        method_ptrs = NEW_C_HEAP_ARRAY(Method*, _methods_count, mtClass);
        for (int i = 0; i < _methods_count; i++) {
            const MethodInfo& mi = _methods[i];

            // 创建 ConstMethod
            ConstMethod* cm = new ConstMethod(
                _cp,
                (u2)mi.code_length,
                mi.max_stack,
                mi.max_locals,
                mi.name_index,
                mi.descriptor_index
            );

            // 拷贝字节码
            if (mi.code_length > 0 && mi.code != nullptr) {
                cm->set_bytecodes(mi.code, mi.code_length);
            }

            // 设置方法编号
            cm->set_method_idnum((u2)i);

            // 创建 Method
            Method* m = new Method(cm, AccessFlags(mi.access_flags));
            m->set_method_idnum((u2)i);

            method_ptrs[i] = m;
        }
    }

    // 3. 创建 InstanceKlass
    InstanceKlass* ik = InstanceKlass::create_from_parser(
        _class_name,
        _super_class_name,
        _access_flags,
        _major_version,
        _minor_version,
        _cp,
        field_entries, _fields_count,
        method_ptrs, _methods_count
    );

    // 4. 转移 ConstantPool 所有权给 InstanceKlass
    //    ClassFileParser 不再负责释放 cp
    _cp = nullptr;

    // 5. MethodInfo.code 的字节码已拷贝到 ConstMethod 中
    //    释放 MethodInfo.code 原始内存
    for (int i = 0; i < _methods_count; i++) {
        if (_methods[i].code != nullptr) {
            FREE_C_HEAP_ARRAY(u1, _methods[i].code);
            _methods[i].code = nullptr;
        }
    }

    return ik;
}

