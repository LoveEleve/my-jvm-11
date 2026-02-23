#ifndef SHARE_INTERPRETER_BYTECODEINTERPRETER_HPP
#define SHARE_INTERPRETER_BYTECODEINTERPRETER_HPP

// ============================================================================
// Mini JVM - C++ 字节码解释器
// 对应 HotSpot: src/hotspot/share/interpreter/bytecodeInterpreter.hpp/.cpp
//
// HotSpot 有两种解释器：
//   1. Template Interpreter — 生成机器码，性能高（默认）
//   2. C++ BytecodeInterpreter — 纯 C++ 实现，可移植
//
// 我们实现纯 C++ 版本，使用 switch-case 派发。
//
// 执行模型：
//   1. 创建 InterpreterFrame（分配局部变量表 + 操作数栈）
//   2. 设置参数到局部变量表
//   3. 进入 switch 循环：读取字节码 → 执行 → 推进 BCP
//   4. 遇到 return/throw → 退出循环
//   5. 遇到 invoke → 递归创建新帧执行
//
// 当前支持的字节码分类：
//   [x] 常量加载：iconst, bipush, sipush, ldc(整数)
//   [x] 局部变量：iload/istore 及 _N 变体, aload/astore
//   [x] 栈操作：  dup, pop, swap
//   [x] 整数算术：iadd, isub, imul, idiv, irem, ineg
//   [x] 位运算：  iand, ior, ixor, ishl, ishr, iushr
//   [x] 自增：    iinc
//   [x] 条件跳转：ifeq/ne/lt/ge/gt/le, if_icmpxx
//   [x] 无条件跳转：goto
//   [x] 方法调用：invokestatic, invokespecial, invokevirtual (简化版)
//   [x] 返回：    ireturn, return
//   [x] 其他：    nop, getstatic/putstatic (输出桩)
// ============================================================================

#include "interpreter/bytecodes.hpp"
#include "runtime/frame.hpp"
#include "runtime/javaThread.hpp"
#include "oops/instanceKlass.hpp"

class BytecodeInterpreter {
public:
    // 执行一个方法
    // method: 要执行的方法
    // klass:  方法所属的 InstanceKlass
    // thread: 当前线程
    // result: 返回值
    // args:   参数数组（局部变量表初始值）
    // args_count: 参数个数
    static void execute(Method* method,
                        InstanceKlass* klass,
                        JavaThread* thread,
                        JavaValue* result,
                        intptr_t* args = nullptr,
                        int args_count = 0);

    // 打印执行的字节码（调试模式）
    static bool _trace_bytecodes;

private:
    // 内部执行循环
    static void run(InterpreterFrame* frame,
                    InstanceKlass* klass,
                    JavaThread* thread,
                    JavaValue* result);

    // 处理方法调用（invokestatic/invokespecial/invokevirtual）
    static void invoke_method(InterpreterFrame* frame,
                              InstanceKlass* klass,
                              JavaThread* thread,
                              u2 cp_index,
                              bool is_static);

    // 处理 getstatic（Phase 5: 真实静态字段访问 + System.out 桩）
    static void handle_getstatic(InterpreterFrame* frame,
                                 InstanceKlass* klass,
                                 u2 cp_index);

    // 处理 putstatic（Phase 5: 真实静态字段写入）
    static void handle_putstatic(InterpreterFrame* frame,
                                 InstanceKlass* klass,
                                 u2 cp_index);

    // 处理 getfield（Phase 5: 真实实例字段读取）
    static void handle_getfield(InterpreterFrame* frame,
                                InstanceKlass* klass,
                                u2 cp_index);

    // 处理 putfield（Phase 5: 真实实例字段写入）
    static void handle_putfield(InterpreterFrame* frame,
                                InstanceKlass* klass,
                                JavaThread* thread,
                                u2 cp_index);

    // 处理 new（Phase 5: 真实对象创建）
    static void handle_new(InterpreterFrame* frame,
                           InstanceKlass* klass,
                           JavaThread* thread,
                           u2 cp_index);

    // 处理 invokevirtual（简化版：仅支持 PrintStream.println 桩）
    static void handle_invokevirtual(InterpreterFrame* frame,
                                     InstanceKlass* klass,
                                     JavaThread* thread,
                                     u2 cp_index,
                                     JavaValue* result);

    // 辅助：解析字段描述符获取字段类型（用于确定读写大小）
    static char resolve_field_type(ConstantPool* cp, u2 cp_index);
};

#endif // SHARE_INTERPRETER_BYTECODEINTERPRETER_HPP
