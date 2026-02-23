#ifndef SHARE_RUNTIME_FRAME_HPP
#define SHARE_RUNTIME_FRAME_HPP

// ============================================================================
// Mini JVM - 解释器栈帧
// 对应 HotSpot: src/hotspot/share/interpreter/bytecodeInterpreter.hpp 中的状态
//              + src/hotspot/share/runtime/frame.hpp
//
// HotSpot 的解释器帧分为两种实现：
//   1. Template Interpreter — 生成机器码的模板解释器（默认）
//   2. C++ BytecodeInterpreter — 纯 C++ 解释器（可选，用于移植）
//
// 我们使用 C++ 解释器方案，帧结构参考 BytecodeInterpreter 的状态字段。
//
// 解释器帧的内存布局：
//
//   高地址 ──────────────────────────────────
//   │ locals[max_locals-1]                  │ ← _locals 指向这里
//   │ locals[max_locals-2]                  │
//   │ ...                                    │
//   │ locals[0]                              │ ← receiver (对于实例方法)
//   ├────────────────────────────────────────┤
//   │ operand_stack[0]                       │ ← _stack_base
//   │ operand_stack[1]                       │
//   │ ...                                    │
//   │ operand_stack[top]                     │ ← _sp (栈顶指针)
//   │ ...                                    │
//   │ operand_stack[max_stack-1]             │ ← _stack_limit
//   低地址 ──────────────────────────────────
//
// 精简实现：
//   - 局部变量表和操作数栈都用 intptr_t 数组
//   - intptr_t 足以存放 int/float/reference（LP64 下 8 字节）
//   - long/double 占用 2 个 slot
// ============================================================================

#include "memory/allocation.hpp"
#include "oops/method.hpp"
#include "oops/constantPool.hpp"

class JavaThread;
class oopDesc;

// ============================================================================
// InterpreterFrame — 单个方法的执行上下文
// ============================================================================

class InterpreterFrame : public CHeapObj<mtThread> {
private:
    // ====== 核心状态 ======
    Method*        _method;       // 当前执行的方法
    ConstantPool*  _constants;    // 当前方法的常量池
    u1*            _bcp;          // Bytecode Pointer（当前执行位置）

    // ====== 局部变量表 ======
    intptr_t*      _locals;       // 局部变量数组
    int            _max_locals;   // 局部变量表大小

    // ====== 操作数栈 ======
    intptr_t*      _stack;        // 操作数栈数组
    int            _sp;           // 栈顶索引（下一个空位，0 = 空栈）
    int            _max_stack;    // 最大栈深度

    // ====== 帧链表 ======
    InterpreterFrame* _caller;    // 调用者帧（nullptr = 最外层）

public:
    InterpreterFrame(Method* method, ConstantPool* cp, InterpreterFrame* caller)
        : _method(method),
          _constants(cp),
          _bcp(method->code_base()),
          _max_locals(method->max_locals()),
          _max_stack(method->max_stack()),
          _sp(0),
          _caller(caller)
    {
        // 分配局部变量表
        _locals = NEW_C_HEAP_ARRAY(intptr_t, _max_locals, mtThread);
        memset(_locals, 0, sizeof(intptr_t) * _max_locals);

        // 分配操作数栈
        _stack = NEW_C_HEAP_ARRAY(intptr_t, _max_stack, mtThread);
        memset(_stack, 0, sizeof(intptr_t) * _max_stack);
    }

    ~InterpreterFrame() {
        if (_locals != nullptr) FREE_C_HEAP_ARRAY(intptr_t, _locals);
        if (_stack != nullptr)  FREE_C_HEAP_ARRAY(intptr_t, _stack);
    }

    // ======== 方法/常量池 ========

    Method*       method()    const { return _method; }
    ConstantPool* constants() const { return _constants; }

    // ======== BCP (字节码指针) ========

    u1*  bcp()        const { return _bcp; }
    void set_bcp(u1* p)    { _bcp = p; }

    // 当前 BCI (字节码索引，相对于 code_base 的偏移)
    int bci() const { return (int)(_bcp - _method->code_base()); }

    // 读取当前字节码
    u1 current_bytecode() const { return *_bcp; }

    // 读取操作数（大端字节序，JVM 规范要求）
    int read_u1_operand(int offset) const {
        return (int)_bcp[offset];
    }

    int read_s1_operand(int offset) const {
        return (jbyte)_bcp[offset];
    }

    int read_u2_operand(int offset) const {
        return ((int)_bcp[offset] << 8) | (int)_bcp[offset + 1];
    }

    int read_s2_operand(int offset) const {
        return (jshort)(((int)_bcp[offset] << 8) | (int)_bcp[offset + 1]);
    }

    // 推进 BCP
    void advance_bcp(int delta) { _bcp += delta; }

    // 设置 BCP 到绝对偏移
    void set_bci(int bci) { _bcp = _method->code_base() + bci; }

    // ======== 局部变量表 ========

    int max_locals() const { return _max_locals; }

    intptr_t  local_int(int index) const {
        vm_assert(index >= 0 && index < _max_locals, "local index out of bounds");
        return _locals[index];
    }

    void set_local_int(int index, intptr_t value) {
        vm_assert(index >= 0 && index < _max_locals, "local index out of bounds");
        _locals[index] = value;
    }

    jlong local_long(int index) const {
        vm_assert(index >= 0 && index + 1 < _max_locals, "local index out of bounds");
        // long 占 2 个 slot：低地址 slot 存值
        return *(jlong*)&_locals[index];
    }

    void set_local_long(int index, jlong value) {
        vm_assert(index >= 0 && index + 1 < _max_locals, "local index out of bounds");
        *(jlong*)&_locals[index] = value;
    }

    jfloat local_float(int index) const {
        vm_assert(index >= 0 && index < _max_locals, "local index out of bounds");
        jint bits = (jint)_locals[index];
        return *(jfloat*)&bits;
    }

    void set_local_float(int index, jfloat value) {
        vm_assert(index >= 0 && index < _max_locals, "local index out of bounds");
        jint bits = *(jint*)&value;
        _locals[index] = (intptr_t)bits;
    }

    jdouble local_double(int index) const {
        vm_assert(index >= 0 && index + 1 < _max_locals, "local index out of bounds");
        return *(jdouble*)&_locals[index];
    }

    void set_local_double(int index, jdouble value) {
        vm_assert(index >= 0 && index + 1 < _max_locals, "local index out of bounds");
        *(jdouble*)&_locals[index] = value;
    }

    oopDesc* local_oop(int index) const {
        vm_assert(index >= 0 && index < _max_locals, "local index out of bounds");
        return (oopDesc*)_locals[index];
    }

    void set_local_oop(int index, oopDesc* value) {
        vm_assert(index >= 0 && index < _max_locals, "local index out of bounds");
        _locals[index] = (intptr_t)value;
    }

    // ======== 操作数栈 ========

    int sp()        const { return _sp; }
    int max_stack() const { return _max_stack; }
    bool stack_is_empty() const { return _sp == 0; }

    // 压入 int
    void push_int(jint value) {
        vm_assert(_sp < _max_stack, "stack overflow");
        _stack[_sp++] = (intptr_t)value;
    }

    jint pop_int() {
        vm_assert(_sp > 0, "stack underflow");
        return (jint)_stack[--_sp];
    }

    jint peek_int(int depth = 0) const {
        vm_assert(_sp - 1 - depth >= 0, "stack underflow");
        return (jint)_stack[_sp - 1 - depth];
    }

    // 压入 long（占 2 slot）
    void push_long(jlong value) {
        vm_assert(_sp + 1 < _max_stack, "stack overflow");
        *(jlong*)&_stack[_sp] = value;
        _sp += 2;
    }

    jlong pop_long() {
        vm_assert(_sp >= 2, "stack underflow");
        _sp -= 2;
        return *(jlong*)&_stack[_sp];
    }

    // 压入 float
    void push_float(jfloat value) {
        vm_assert(_sp < _max_stack, "stack overflow");
        jint bits = *(jint*)&value;
        _stack[_sp++] = (intptr_t)bits;
    }

    jfloat pop_float() {
        vm_assert(_sp > 0, "stack underflow");
        jint bits = (jint)_stack[--_sp];
        return *(jfloat*)&bits;
    }

    // 压入 double（占 2 slot）
    void push_double(jdouble value) {
        vm_assert(_sp + 1 < _max_stack, "stack overflow");
        *(jdouble*)&_stack[_sp] = value;
        _sp += 2;
    }

    jdouble pop_double() {
        vm_assert(_sp >= 2, "stack underflow");
        _sp -= 2;
        return *(jdouble*)&_stack[_sp];
    }

    // 压入/弹出 oop
    void push_oop(oopDesc* value) {
        vm_assert(_sp < _max_stack, "stack overflow");
        _stack[_sp++] = (intptr_t)value;
    }

    oopDesc* pop_oop() {
        vm_assert(_sp > 0, "stack underflow");
        return (oopDesc*)_stack[--_sp];
    }

    // 通用 slot 操作（dup/swap 等需要）
    intptr_t pop_raw() {
        vm_assert(_sp > 0, "stack underflow");
        return _stack[--_sp];
    }

    void push_raw(intptr_t value) {
        vm_assert(_sp < _max_stack, "stack overflow");
        _stack[_sp++] = value;
    }

    intptr_t peek_raw(int depth = 0) const {
        vm_assert(_sp - 1 - depth >= 0, "stack underflow");
        return _stack[_sp - 1 - depth];
    }

    // ======== 调用者帧 ========

    InterpreterFrame* caller() const { return _caller; }

    // ======== 调试打印 ========

    void print_on(FILE* out) const {
        fprintf(out, "  Frame: method=%s, bci=%d, sp=%d/%d, locals=%d\n",
                _method->internal_name(),
                bci(), _sp, _max_stack, _max_locals);
        // 打印局部变量
        fprintf(out, "    locals: [");
        for (int i = 0; i < _max_locals; i++) {
            if (i > 0) fprintf(out, ", ");
            fprintf(out, "%ld", (long)_locals[i]);
        }
        fprintf(out, "]\n");
        // 打印操作数栈
        fprintf(out, "    stack:  [");
        for (int i = 0; i < _sp; i++) {
            if (i > 0) fprintf(out, ", ");
            fprintf(out, "%ld", (long)_stack[i]);
        }
        fprintf(out, "]\n");
    }

    NONCOPYABLE(InterpreterFrame);
};

#endif // SHARE_RUNTIME_FRAME_HPP
