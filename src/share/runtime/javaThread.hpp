#ifndef SHARE_RUNTIME_JAVATHREAD_HPP
#define SHARE_RUNTIME_JAVATHREAD_HPP

// ============================================================================
// Mini JVM - JavaThread（Java 线程）
// 对应 HotSpot: src/hotspot/share/runtime/thread.hpp
//
// JavaThread 是 JVM 内部表示一个执行线程的核心数据结构。
// 每个执行 Java 代码的线程都有一个 JavaThread 对象。
//
// HotSpot 继承层次：
//   ThreadShadow (异常相关)
//   └── Thread (通用线程)
//       └── JavaThread (Java 线程)
//           ├── CompilerThread
//           └── ServiceThread
//
// 我们的精简版：
//   - 不做多线程（单线程解释执行即可）
//   - 保留核心字段：线程状态、帧锚点、异常、VM 结果
//   - 不需要 OS 线程创建逻辑
// ============================================================================

#include "memory/allocation.hpp"
#include "oops/method.hpp"

class InstanceKlass;
class oopDesc;

// ============================================================================
// JavaFrameAnchor — Java/Native 帧边界锚点
// 对应 HotSpot: src/hotspot/share/runtime/javaFrameAnchor.hpp
//
// 连接 Java 栈和 C++ 栈的关键桥梁。
// 当 Java 调用 native（或反过来）时，保存当前帧状态。
// ============================================================================

class JavaFrameAnchor {
public:
    intptr_t* _last_Java_sp;     // 最后一个 Java 帧的栈指针
    address   _last_Java_pc;     // 最后一个 Java 帧的 PC

    JavaFrameAnchor() : _last_Java_sp(nullptr), _last_Java_pc(nullptr) {}

    void clear() {
        _last_Java_sp = nullptr;
        _last_Java_pc = nullptr;
    }

    bool has_last_Java_frame() const { return _last_Java_sp != nullptr; }
};

// ============================================================================
// JavaValue — 方法调用返回值
// 对应 HotSpot: src/hotspot/share/utilities/globalDefinitions.hpp 中的 JavaValue
//
// 统一封装所有可能的返回值类型。
// ============================================================================

class JavaValue {
public:
    BasicType _type;

    union {
        jint     i;
        jlong    l;
        jfloat   f;
        jdouble  d;
        oopDesc* o;     // 对象引用
    } _value;

    JavaValue() : _type(T_VOID) { _value.l = 0; }
    JavaValue(BasicType t) : _type(t) { _value.l = 0; }

    BasicType get_type()    const { return _type; }
    jint      get_jint()    const { return _value.i; }
    jlong     get_jlong()   const { return _value.l; }
    jfloat    get_jfloat()  const { return _value.f; }
    jdouble   get_jdouble() const { return _value.d; }
    oopDesc*  get_oop()     const { return _value.o; }

    void set_jint(jint v)      { _value.i = v; _type = T_INT; }
    void set_jlong(jlong v)    { _value.l = v; _type = T_LONG; }
    void set_jfloat(jfloat v)  { _value.f = v; _type = T_FLOAT; }
    void set_jdouble(jdouble v){ _value.d = v; _type = T_DOUBLE; }
    void set_oop(oopDesc* v)   { _value.o = v; _type = T_OBJECT; }
    void set_type(BasicType t) { _type = t; }
};

// ============================================================================
// JavaThread — Java 线程
// ============================================================================

class JavaThread : public CHeapObj<mtThread> {
private:
    // 线程状态
    volatile JavaThreadState _thread_state;

    // 帧锚点 — Java/Native 边界
    JavaFrameAnchor _anchor;

    // 异常相关
    oopDesc* _pending_exception;      // 待处理异常对象
    const char* _exception_message;   // 异常消息（简化版）

    // VM 调用结果
    oopDesc*  _vm_result;       // oop 结果（GC safe）
    void*     _vm_result_2;     // 非 oop 结果

    // 当前正在执行的方法（调试用）
    Method*   _current_method;

    // 线程名称
    const char* _name;

public:
    JavaThread(const char* name = "main")
        : _thread_state(_thread_new),
          _anchor(),
          _pending_exception(nullptr),
          _exception_message(nullptr),
          _vm_result(nullptr),
          _vm_result_2(nullptr),
          _current_method(nullptr),
          _name(name)
    {}

    ~JavaThread() {}

    // ======== 线程状态 ========

    JavaThreadState thread_state() const { return _thread_state; }
    void set_thread_state(JavaThreadState s) { _thread_state = s; }

    bool is_in_java()   const { return _thread_state == _thread_in_Java; }
    bool is_in_vm()     const { return _thread_state == _thread_in_vm; }
    bool is_in_native() const { return _thread_state == _thread_in_native; }

    // ======== 帧锚点 ========

    JavaFrameAnchor& frame_anchor() { return _anchor; }
    bool has_last_Java_frame() const { return _anchor.has_last_Java_frame(); }

    // ======== 异常 ========

    bool has_pending_exception() const { return _pending_exception != nullptr; }
    oopDesc* pending_exception() const { return _pending_exception; }
    const char* exception_message() const { return _exception_message; }

    void set_pending_exception(oopDesc* e, const char* msg = nullptr) {
        _pending_exception = e;
        _exception_message = msg;
    }

    void clear_pending_exception() {
        _pending_exception = nullptr;
        _exception_message = nullptr;
    }

    // ======== VM 结果 ========

    oopDesc* vm_result() const { return _vm_result; }
    void set_vm_result(oopDesc* r) { _vm_result = r; }

    // ======== 当前方法 ========

    Method* current_method() const { return _current_method; }
    void set_current_method(Method* m) { _current_method = m; }

    // ======== 线程名 ========

    const char* name() const { return _name; }

    // ======== 调试 ========

    void print_on(FILE* out) const {
        fprintf(out, "JavaThread(%p) name=\"%s\" state=%d",
                (void*)this, _name, (int)_thread_state);
        if (_pending_exception) {
            fprintf(out, " [exception pending: %s]",
                    _exception_message ? _exception_message : "");
        }
    }

    NONCOPYABLE(JavaThread);
};

#endif // SHARE_RUNTIME_JAVATHREAD_HPP
