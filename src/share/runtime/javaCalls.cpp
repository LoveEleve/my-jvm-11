#include "runtime/javaCalls.hpp"
#include "interpreter/bytecodeInterpreter.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/method.hpp"
#include "runtime/javaThread.hpp"

#include <cstdio>

// ============================================================================
// JavaCalls — C++ → Java 方法调用桥梁
// 对照 HotSpot: src/hotspot/share/runtime/javaCalls.cpp
// ============================================================================

void JavaCalls::call(JavaValue* result,
                    InstanceKlass* klass,
                    Method* method,
                    JavaThread* thread,
                    intptr_t* args,
                    int args_count) {
    // 对照: JavaCalls::call() [javaCalls.cpp:339]
    //   → os::os_exception_wrapper(call_helper, ...)
    //     → call_helper() [javaCalls.cpp:348]
    //
    // call_helper() 做的事:
    //   1. CompilationPolicy::compile_if_required(method)  → JIT 编译触发 (跳过)
    //   2. entry_point = method->from_interpreted_entry()
    //   3. 栈溢出检查
    //   4. StubRoutines::call_stub()(...) → 跳转到解释器
    //
    // 我们简化为直接调用 BytecodeInterpreter::execute()

    vm_assert(method != nullptr, "method is null");
    vm_assert(klass != nullptr, "klass is null");
    vm_assert(thread != nullptr, "thread is null");

    // 保存之前的线程状态
    JavaThreadState old_state = thread->thread_state();
    thread->set_thread_state(_thread_in_Java);

    // 调用解释器执行
    BytecodeInterpreter::execute(method, klass, thread, result, args, args_count);

    // 恢复线程状态
    thread->set_thread_state(old_state);
}

void JavaCalls::call_static(JavaValue* result,
                           InstanceKlass* klass,
                           Method* method,
                           JavaThread* thread,
                           intptr_t* args,
                           int args_count) {
    // 对照: JavaCalls::call_static() [javaCalls.cpp:305]
    // HotSpot 中 call_static 会构建 JavaCallArguments 然后调用 call()
    // 我们直接转发
    call(result, klass, method, thread, args, args_count);
}

void JavaCalls::call_virtual(JavaValue* result,
                            InstanceKlass* klass,
                            Method* method,
                            JavaThread* thread,
                            intptr_t* args,
                            int args_count) {
    // 对照: JavaCalls::call_virtual() [javaCalls.cpp:269]
    // HotSpot 中会通过 vtable 进行虚分派
    // 我们暂时直接调用指定方法（Phase 16 完善虚分派）
    call(result, klass, method, thread, args, args_count);
}
