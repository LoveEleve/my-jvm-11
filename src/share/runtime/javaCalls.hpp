#ifndef SHARE_RUNTIME_JAVACALLS_HPP
#define SHARE_RUNTIME_JAVACALLS_HPP

// ============================================================================
// Mini JVM - JavaCalls（C++ → Java 方法调用桥梁）
// 对照 HotSpot: src/hotspot/share/runtime/javaCalls.hpp
//
// JavaCalls 是从 C++ 代码调用 Java 方法的统一入口。
//
// HotSpot 中的调用链:
//   JavaCalls::call() [javaCalls.cpp:339]
//     → call_helper() [javaCalls.cpp:348]
//         → CompilationPolicy::compile_if_required()  (触发JIT)
//         → entry_point = method->from_interpreted_entry()
//         → StubRoutines::call_stub()(...)
//             → call_stub 是预生成的机器码
//             → 设置 Java 栈帧 → 跳转到解释器入口
//             → 解释器开始执行字节码
//
// 我们的简化版:
//   JavaCalls::call() → BytecodeInterpreter::execute()
//   不需要 call_stub，直接调用 C++ 解释器
// ============================================================================

#include "utilities/globalDefinitions.hpp"

class Method;
class InstanceKlass;
class JavaThread;
class JavaValue;

class JavaCalls {
public:
    // 调用方法（通用入口）
    // 对照: JavaCalls::call() [javaCalls.cpp:339]
    //
    // 内部流程:
    //   1. 检查方法有效性
    //   2. (HotSpot: 触发 JIT 编译 → 跳过)
    //   3. 调用 BytecodeInterpreter::execute()
    static void call(JavaValue* result,
                    InstanceKlass* klass,
                    Method* method,
                    JavaThread* thread,
                    intptr_t* args = nullptr,
                    int args_count = 0);

    // 调用静态方法
    // 对照: JavaCalls::call_static() [javaCalls.cpp:305]
    static void call_static(JavaValue* result,
                           InstanceKlass* klass,
                           Method* method,
                           JavaThread* thread,
                           intptr_t* args = nullptr,
                           int args_count = 0);

    // 调用实例方法
    // 对照: JavaCalls::call_virtual() [javaCalls.cpp:269]
    static void call_virtual(JavaValue* result,
                            InstanceKlass* klass,
                            Method* method,
                            JavaThread* thread,
                            intptr_t* args,
                            int args_count);
};

#endif // SHARE_RUNTIME_JAVACALLS_HPP
