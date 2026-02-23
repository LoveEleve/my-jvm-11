#ifndef SHARE_OOPS_CONSTMETHOD_HPP
#define SHARE_OOPS_CONSTMETHOD_HPP

// ============================================================================
// Mini JVM - ConstMethod（方法的不可变部分）
// 对应 HotSpot: src/hotspot/share/oops/constMethod.hpp
//
// ConstMethod 存储方法中在 classfile 解析后不会改变的数据：
//   - 字节码
//   - 行号表（压缩）
//   - 异常表
//   - 局部变量表
//   - 方法参数
//   等
//
// HotSpot 设计要点：
//   1. ConstMethod 不能有虚函数（为了 CDS 共享，vptr 会变）
//   2. 字节码和各种表都内嵌在 ConstMethod 对象之后
//   3. 从前往后布局：[ConstMethod 字段] [字节码] [行号表]
//      从后往前索引：[局部变量表] [异常表] [checked exceptions] [方法参数]
//
// 内存布局：
//   ┌────────────────────────────────────────────────────┐
//   │ ConstMethod declared fields                        │ ← this
//   ├────────────────────────────────────────────────────┤
//   │ [EMBEDDED bytecodes]                               │ ← code_base()
//   │    (code_size bytes)                               │
//   ├────────────────────────────────────────────────────┤
//   │ [EMBEDDED compressed line number table]            │ (if present)
//   ├────────────────────────────────────────────────────┤
//   │         ... possible padding ...                   │
//   ├────────────────────────────────────────────────────┤
//   │ [EMBEDDED exception table + length]                │ ← indexed from end
//   │ [EMBEDDED checked exceptions + length]             │
//   │ [EMBEDDED generic signature index]                 │
//   └────────────────────────────────────────────────────┘
//
// 我们的精简版：
//   - 使用单独分配的 u1 数组存储字节码（不做内嵌）
//   - 后续可以改为内嵌布局以匹配 HotSpot
//   - 继承 CHeapObj（暂不用 MetaspaceObj）
// ============================================================================

#include "memory/allocation.hpp"
#include "utilities/accessFlags.hpp"

class ConstantPool;
class Method;

// 异常表元素
struct ExceptionTableElement {
    u2 start_pc;
    u2 end_pc;
    u2 handler_pc;
    u2 catch_type_index;
};

class ConstMethod : public CHeapObj<mtClass> {
    friend class Method;

public:
    // 方法类型
    enum MethodType { NORMAL, OVERPASS };

private:
    // ========== 内部标志 ==========
    // HotSpot: constMethod.hpp 约 179-191 行
    // 标记哪些可选表存在

    enum {
        _has_linenumber_table      = 0x0001,
        _has_checked_exceptions    = 0x0002,
        _has_localvariable_table   = 0x0004,
        _has_exception_table       = 0x0008,
        _has_generic_signature     = 0x0010,
        _has_method_parameters     = 0x0020,
        _is_overpass               = 0x0040,
    };

    // ========== 字段 ==========
    // HotSpot: constMethod.hpp 约 199-229 行

    ConstantPool* _constants;      // 所属常量池（回指）
    int           _constMethod_size; // 整体大小（以 word 为单位）
    u2            _flags;          // 内部标志
    u1            _result_type;    // BasicType of return type

    u2            _code_size;      // 字节码长度
    u2            _name_index;     // 方法名（常量池索引）
    u2            _signature_index;// 方法签名（常量池索引）
    u2            _method_idnum;   // 方法在类中的唯一编号

    u2            _max_stack;      // 操作数栈最大深度
    u2            _max_locals;     // 局部变量表大小
    u2            _size_of_parameters; // 参数块大小（receiver + args，word 数）

    // ========== 字节码存储 ==========
    // 精简版：独立分配，不内嵌
    u1*           _bytecodes;      // 字节码数组（堆分配）

    // ========== 异常表 ==========
    int                    _exception_table_length;
    ExceptionTableElement* _exception_table;  // 堆分配

public:
    // ======== 构造/析构 ========

    ConstMethod(ConstantPool* constants, u2 code_size, u2 max_stack, u2 max_locals,
                u2 name_index, u2 signature_index)
        : _constants(constants),
          _constMethod_size(0),
          _flags(0),
          _result_type(0),
          _code_size(code_size),
          _name_index(name_index),
          _signature_index(signature_index),
          _method_idnum(0),
          _max_stack(max_stack),
          _max_locals(max_locals),
          _size_of_parameters(0),
          _bytecodes(nullptr),
          _exception_table_length(0),
          _exception_table(nullptr)
    {
        if (code_size > 0) {
            _bytecodes = NEW_C_HEAP_ARRAY(u1, code_size, mtCode);
            memset(_bytecodes, 0, code_size);
        }
    }

    ~ConstMethod() {
        if (_bytecodes != nullptr) {
            FREE_C_HEAP_ARRAY(u1, _bytecodes);
        }
        if (_exception_table != nullptr) {
            FREE_C_HEAP_ARRAY(ExceptionTableElement, _exception_table);
        }
    }

    // ======== 常量池 ========

    ConstantPool* constants() const { return _constants; }
    void set_constants(ConstantPool* cp) { _constants = cp; }

    // ======== 字节码访问 ========

    u2 code_size() const { return _code_size; }

    // 获取字节码数组
    u1* code_base() const { return _bytecodes; }

    // 获取某个位置的字节码
    u1 bytecode_at(int bci) const {
        vm_assert(bci >= 0 && bci < _code_size, "bci out of bounds");
        return _bytecodes[bci];
    }

    // 拷贝字节码
    void set_bytecodes(const u1* code, int length) {
        vm_assert(length == _code_size, "code size mismatch");
        memcpy(_bytecodes, code, length);
    }

    // ======== 方法名/签名 ========

    u2 name_index()      const { return _name_index; }
    u2 signature_index() const { return _signature_index; }

    void set_name_index(u2 idx)      { _name_index = idx; }
    void set_signature_index(u2 idx) { _signature_index = idx; }

    // ======== 栈/局部变量 ========

    u2 max_stack()  const { return _max_stack; }
    u2 max_locals() const { return _max_locals; }

    void set_max_stack(u2 v)  { _max_stack = v; }
    void set_max_locals(u2 v) { _max_locals = v; }

    u2 size_of_parameters() const { return _size_of_parameters; }
    void set_size_of_parameters(u2 v) { _size_of_parameters = v; }

    // ======== 方法编号 ========

    u2 method_idnum() const { return _method_idnum; }
    void set_method_idnum(u2 v) { _method_idnum = v; }

    // ======== 返回类型 ========

    BasicType result_type() const { return (BasicType)_result_type; }
    void set_result_type(BasicType t) { _result_type = (u1)t; }

    // ======== 内部标志 ========

    bool has_linenumber_table()    const { return (_flags & _has_linenumber_table) != 0; }
    bool has_checked_exceptions()  const { return (_flags & _has_checked_exceptions) != 0; }
    bool has_localvariable_table() const { return (_flags & _has_localvariable_table) != 0; }
    bool has_exception_table()     const { return (_flags & _has_exception_table) != 0; }
    bool has_generic_signature()   const { return (_flags & _has_generic_signature) != 0; }
    bool is_overpass()             const { return (_flags & _is_overpass) != 0; }

    void set_has_linenumber_table()    { _flags |= _has_linenumber_table; }
    void set_has_checked_exceptions()  { _flags |= _has_checked_exceptions; }
    void set_has_localvariable_table() { _flags |= _has_localvariable_table; }
    void set_has_exception_table()     { _flags |= _has_exception_table; }

    // ======== 异常表 ========

    int exception_table_length() const { return _exception_table_length; }
    ExceptionTableElement* exception_table_start() const { return _exception_table; }

    void set_exception_table(ExceptionTableElement* table, int length) {
        _exception_table = table;
        _exception_table_length = length;
        if (length > 0) {
            _flags |= _has_exception_table;
        }
    }

    // ======== 调试打印 ========

    void print_on(FILE* out) const {
        fprintf(out, "ConstMethod(%p): name_index=%d, sig_index=%d, "
                     "code_size=%d, max_stack=%d, max_locals=%d",
                (void*)this, _name_index, _signature_index,
                _code_size, _max_stack, _max_locals);
    }

    NONCOPYABLE(ConstMethod);
};

#endif // SHARE_OOPS_CONSTMETHOD_HPP
