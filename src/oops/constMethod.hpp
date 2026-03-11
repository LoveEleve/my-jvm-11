/*
 * my_jvm - ConstMethod (Method Read-Only Data)
 *
 * 方法的只读数据，来自 OpenJDK 11 hotspot/src/share/vm/oops/constMethod.hpp
 * 对标 OpenJDK 11 slowdebug 版本
 *
 * 关键设计：
 * 1. 继承 MetaspaceObj（不是 Metadata！）
 * 2. 字节码内联存储在结构体之后（无 _code 指针）
 *    访问方式：code_base() = (u1*)(this + 1)
 *
 * 内存布局（64-bit）：
 *   [0]   vtable ptr (8)
 *   [8]   _fingerprint: uint64_t (8)
 *   [16]  _constants: ConstantPool* (8)
 *   [24]  _stackmap_data: Array<u1>* (8)
 *   [32]  _adapter: AdapterHandlerEntry* (8)  ← union
 *   [40]  _constMethod_size: int (4)
 *   [44]  _flags: u2 (2)
 *   [46]  _result_type: u1 (1) + padding (1)
 *   [48]  _code_size: u2 (2)
 *   [50]  _name_index: u2 (2)
 *   [52]  _signature_index: u2 (2)
 *   [54]  _method_idnum: u2 (2)
 *   ... 后续字段内联在 56 字节之后
 *   sizeof(ConstMethod) = 56  ← GDB 验证
 *
 *   [56+] 字节码（内联，紧跟结构体之后）
 */

#ifndef MY_JVM_OOPS_CONSTMETHOD_HPP
#define MY_JVM_OOPS_CONSTMETHOD_HPP

#include <cstring>
#include "globalDefinitions.hpp"
#include "metadata.hpp"

// ========== 前向声明 ==========

class ConstantPool;
template <class T> class Array;
class AdapterHandlerEntry;

// ========== ConstMethod 类 ==========
// 方法的只读数据，存在 Metaspace 中
// 继承 MetaspaceObj（不是 Metadata！）
// 参考：constMethod.hpp 第 171 行

// ConstMethod 不继承 MetaspaceObj（OpenJDK 中 MetaspaceObj 无虚函数，空基类优化使 sizeof=0）
// 在 my_jvm 中 MetaspaceObj 有虚析构函数，为保持布局一致，ConstMethod 直接作为独立类
class ConstMethod {
public:
    typedef enum { NORMAL, OVERPASS } MethodType;

private:
    // ========== 字段（严格按 OpenJDK 源码顺序）==========

    // 签名指纹（用于 AOT 验证）
    // 参考：constMethod.hpp 第 183 行
    volatile uint64_t _fingerprint;

    // 常量池指针
    // 参考：constMethod.hpp 第 188 行
    ConstantPool*   _constants;

    // 原始 stackmap 数据
    // 参考：constMethod.hpp 第 191 行
    Array<u1>*      _stackmap_data;

    // 适配器（i2c/c2i），方法链接时设置一次
    // 参考：constMethod.hpp 第 194-197 行
    union {
        AdapterHandlerEntry* _adapter;
        AdapterHandlerEntry** _adapter_trampoline;
    };

    // ConstMethod 总大小（单位：word）
    int             _constMethod_size;

    // 标志位（各种内联表的存在标志）
    u2              _flags;

    // 返回类型（BasicType）
    u1              _result_type;

    // ========== 以下字段从 offset 48 开始 ==========

    // 字节码大小（字节数）
    // 注意：字节码本身内联存储在结构体之后，通过 code_base() 访问
    u2              _code_size;

    // 方法名在常量池中的索引
    u2              _name_index;

    // 方法签名在常量池中的索引
    u2              _signature_index;

    // 方法在类中的唯一 ID（对应 methods 数组的初始索引）
    u2              _method_idnum;

    // 操作数栈最大深度
    u2              _max_stack;

    // 本地变量表最大槽数
    u2              _max_locals;

    // 参数块大小（接收者 + 参数，单位：word）
    u2              _size_of_parameters;

    // 原始方法 ID（用于 RedefineClasses）
    u2              _orig_method_idnum;

public:
    // ========== 构造函数 ==========

    ConstMethod() : _fingerprint(0), _constants(nullptr),
                    _stackmap_data(nullptr), _adapter(nullptr),
                    _constMethod_size(0), _flags(0), _result_type(0),
                    _code_size(0), _name_index(0), _signature_index(0),
                    _method_idnum(0), _max_stack(0), _max_locals(0),
                    _size_of_parameters(0), _orig_method_idnum(0) {}

    // ========== 类型判断 ==========

    bool is_constMethod() const { return true; }

    // ========== 字节码访问（内联存储）==========
    //
    // 关键设计：字节码紧跟在 ConstMethod 结构体之后，无独立指针
    // 这与 my_jvm 旧版本（有 _code 指针）完全不同
    // 参考：constMethod.hpp 第 430 行：address code_base() const { return (address)(this+1); }

    u1* code_base() const { return (u1*)(this + 1); }
    u1* code_end()  const { return code_base() + _code_size; }

    // 设置字节码（从外部 buffer 拷贝到内联区域）
    void set_code(address code) {
        if (_code_size > 0) {
            memcpy((void*)code_base(), (const void*)code, _code_size);
        }
    }

    bool contains(u1* bcp) const {
        return code_base() <= bcp && bcp < code_end();
    }

    // ========== 名称和签名索引 ==========

    u2 name_index() const { return _name_index; }
    void set_name_index(u2 index) { _name_index = index; }

    u2 signature_index() const { return _signature_index; }
    void set_signature_index(u2 index) { _signature_index = index; }

    // ========== 字节码大小 ==========

    int code_size() const { return _code_size; }
    void set_code_size(u2 size) { _code_size = size; }

    // ========== 栈帧信息 ==========

    u2 max_stack() const { return _max_stack; }
    void set_max_stack(u2 stack) { _max_stack = stack; }

    u2 max_locals() const { return _max_locals; }
    void set_max_locals(u2 locals) { _max_locals = locals; }

    // ========== 参数信息 ==========

    u2 size_of_parameters() const { return _size_of_parameters; }
    void set_size_of_parameters(u2 size) { _size_of_parameters = size; }

    // ========== 常量池 ==========

    ConstantPool* constants() const { return _constants; }
    void set_constants(ConstantPool* c) { _constants = c; }

    // ========== stackmap ==========

    Array<u1>* stackmap_data() const { return _stackmap_data; }
    void set_stackmap_data(Array<u1>* sd) { _stackmap_data = sd; }
    bool has_stackmap_table() const { return _stackmap_data != nullptr; }

    // ========== 方法 ID ==========

    u2 method_idnum() const { return _method_idnum; }
    void set_method_idnum(u2 idnum) { _method_idnum = idnum; }

    u2 orig_method_idnum() const { return _orig_method_idnum; }
    void set_orig_method_idnum(u2 idnum) { _orig_method_idnum = idnum; }

    // ========== 方法类型 ==========

    MethodType method_type() const {
        return ((_flags & 0x0040) == 0) ? NORMAL : OVERPASS;
    }

    // ========== 大小 ==========

    int constMethod_size() const { return _constMethod_size; }
    void set_constMethod_size(int size) { _constMethod_size = size; }
};

#endif // MY_JVM_OOPS_CONSTMETHOD_HPP
