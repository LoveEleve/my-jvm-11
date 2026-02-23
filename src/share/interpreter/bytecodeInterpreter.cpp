// ============================================================================
// Mini JVM - C++ 字节码解释器实现
// 对应 HotSpot: src/hotspot/share/interpreter/bytecodeInterpreter.cpp
//
// 使用 switch-case 派发，逐条执行字节码。
// HotSpot 的 C++ 解释器在编译时可选择 computed goto 优化，
// 我们使用标准 switch-case，清晰易懂。
// ============================================================================

#include "interpreter/bytecodeInterpreter.hpp"
#include "gc/shared/javaHeap.hpp"
#include "oops/typeArrayKlass.hpp"
#include "oops/typeArrayOop.hpp"

bool BytecodeInterpreter::_trace_bytecodes = false;

// ============================================================================
// execute() — 外部入口
// ============================================================================

void BytecodeInterpreter::execute(Method* method,
                                  InstanceKlass* klass,
                                  JavaThread* thread,
                                  JavaValue* result,
                                  intptr_t* args,
                                  int args_count)
{
    guarantee(method != nullptr, "method must not be null");
    guarantee(klass != nullptr, "klass must not be null");
    guarantee(thread != nullptr, "thread must not be null");

    // 设置线程状态
    thread->set_thread_state(_thread_in_Java);
    thread->set_current_method(method);

    // 创建栈帧
    InterpreterFrame frame(method, klass->constants(), nullptr);

    // 设置参数到局部变量表
    if (args != nullptr) {
        for (int i = 0; i < args_count && i < method->max_locals(); i++) {
            frame.set_local_int(i, args[i]);
        }
    }

    // 执行
    run(&frame, klass, thread, result);

    // 恢复线程状态
    thread->set_current_method(nullptr);
    thread->set_thread_state(_thread_in_vm);
}

// ============================================================================
// run() — 核心执行循环
//
// 这是整个解释器的心脏。
// 对应 HotSpot: BytecodeInterpreter::run(interpreterState istate)
//
// 执行流程：
//   1. 读取当前字节码 (*bcp)
//   2. switch 到对应处理逻辑
//   3. 操作操作数栈和局部变量表
//   4. 推进 bcp 到下一条指令
//   5. 重复
// ============================================================================

void BytecodeInterpreter::run(InterpreterFrame* frame,
                               InstanceKlass* klass,
                               JavaThread* thread,
                               JavaValue* result)
{
    while (true) {
        // 检查异常
        if (thread->has_pending_exception()) {
            if (_trace_bytecodes) {
                fprintf(stderr, "  [EXCEPTION] %s\n",
                        thread->exception_message() ? thread->exception_message() : "");
            }
            return;
        }

        u1 opcode = frame->current_bytecode();

        if (_trace_bytecodes) {
            fprintf(stderr, "  [%3d] %-16s sp=%d\n",
                    frame->bci(),
                    Bytecodes::name((Bytecodes::Code)opcode),
                    frame->sp());
        }

        switch (opcode) {

        // ================================================================
        // 常量加载
        // ================================================================

        case Bytecodes::_nop:
            frame->advance_bcp(1);
            break;

        case Bytecodes::_aconst_null:
            frame->push_oop(nullptr);
            frame->advance_bcp(1);
            break;

        case Bytecodes::_iconst_m1:
            frame->push_int(-1);
            frame->advance_bcp(1);
            break;
        case Bytecodes::_iconst_0:
            frame->push_int(0);
            frame->advance_bcp(1);
            break;
        case Bytecodes::_iconst_1:
            frame->push_int(1);
            frame->advance_bcp(1);
            break;
        case Bytecodes::_iconst_2:
            frame->push_int(2);
            frame->advance_bcp(1);
            break;
        case Bytecodes::_iconst_3:
            frame->push_int(3);
            frame->advance_bcp(1);
            break;
        case Bytecodes::_iconst_4:
            frame->push_int(4);
            frame->advance_bcp(1);
            break;
        case Bytecodes::_iconst_5:
            frame->push_int(5);
            frame->advance_bcp(1);
            break;

        case Bytecodes::_bipush: {
            jbyte value = (jbyte)frame->read_u1_operand(1);
            frame->push_int((jint)value);
            frame->advance_bcp(2);
            break;
        }

        case Bytecodes::_sipush: {
            jshort value = (jshort)frame->read_s2_operand(1);
            frame->push_int((jint)value);
            frame->advance_bcp(3);
            break;
        }

        case Bytecodes::_ldc: {
            // 简化版：只支持 Integer 常量
            u1 index = frame->read_u1_operand(1);
            ConstantPool* cp = frame->constants();
            jbyte tag = cp->tag_at(index).value();
            if (tag == JVM_CONSTANT_Integer) {
                frame->push_int(cp->int_at(index));
            } else if (tag == JVM_CONSTANT_String || tag == JVM_CONSTANT_StringIndex) {
                // String 常量暂不支持，压入 null
                frame->push_oop(nullptr);
            } else {
                fprintf(stderr, "WARNING: ldc unsupported tag %d at cp#%d\n", tag, index);
                frame->push_int(0);
            }
            frame->advance_bcp(2);
            break;
        }

        // ================================================================
        // 局部变量加载
        // ================================================================

        case Bytecodes::_iload: {
            int index = frame->read_u1_operand(1);
            frame->push_int((jint)frame->local_int(index));
            frame->advance_bcp(2);
            break;
        }

        case Bytecodes::_iload_0:
            frame->push_int((jint)frame->local_int(0));
            frame->advance_bcp(1);
            break;
        case Bytecodes::_iload_1:
            frame->push_int((jint)frame->local_int(1));
            frame->advance_bcp(1);
            break;
        case Bytecodes::_iload_2:
            frame->push_int((jint)frame->local_int(2));
            frame->advance_bcp(1);
            break;
        case Bytecodes::_iload_3:
            frame->push_int((jint)frame->local_int(3));
            frame->advance_bcp(1);
            break;

        case Bytecodes::_aload: {
            int index = frame->read_u1_operand(1);
            frame->push_oop(frame->local_oop(index));
            frame->advance_bcp(2);
            break;
        }

        case Bytecodes::_aload_0:
            frame->push_oop(frame->local_oop(0));
            frame->advance_bcp(1);
            break;
        case Bytecodes::_aload_1:
            frame->push_oop(frame->local_oop(1));
            frame->advance_bcp(1);
            break;
        case Bytecodes::_aload_2:
            frame->push_oop(frame->local_oop(2));
            frame->advance_bcp(1);
            break;
        case Bytecodes::_aload_3:
            frame->push_oop(frame->local_oop(3));
            frame->advance_bcp(1);
            break;

        // ================================================================
        // 局部变量存储
        // ================================================================

        case Bytecodes::_istore: {
            int index = frame->read_u1_operand(1);
            frame->set_local_int(index, frame->pop_int());
            frame->advance_bcp(2);
            break;
        }

        case Bytecodes::_istore_0:
            frame->set_local_int(0, frame->pop_int());
            frame->advance_bcp(1);
            break;
        case Bytecodes::_istore_1:
            frame->set_local_int(1, frame->pop_int());
            frame->advance_bcp(1);
            break;
        case Bytecodes::_istore_2:
            frame->set_local_int(2, frame->pop_int());
            frame->advance_bcp(1);
            break;
        case Bytecodes::_istore_3:
            frame->set_local_int(3, frame->pop_int());
            frame->advance_bcp(1);
            break;

        case Bytecodes::_astore: {
            int index = frame->read_u1_operand(1);
            frame->set_local_oop(index, frame->pop_oop());
            frame->advance_bcp(2);
            break;
        }

        case Bytecodes::_astore_0:
            frame->set_local_oop(0, frame->pop_oop());
            frame->advance_bcp(1);
            break;
        case Bytecodes::_astore_1:
            frame->set_local_oop(1, frame->pop_oop());
            frame->advance_bcp(1);
            break;
        case Bytecodes::_astore_2:
            frame->set_local_oop(2, frame->pop_oop());
            frame->advance_bcp(1);
            break;
        case Bytecodes::_astore_3:
            frame->set_local_oop(3, frame->pop_oop());
            frame->advance_bcp(1);
            break;

        // ================================================================
        // 栈操作
        // ================================================================

        case Bytecodes::_pop:
            frame->pop_raw();
            frame->advance_bcp(1);
            break;

        case Bytecodes::_dup: {
            intptr_t val = frame->peek_raw(0);
            frame->push_raw(val);
            frame->advance_bcp(1);
            break;
        }

        case Bytecodes::_dup_x1: {
            intptr_t val1 = frame->pop_raw();
            intptr_t val2 = frame->pop_raw();
            frame->push_raw(val1);
            frame->push_raw(val2);
            frame->push_raw(val1);
            frame->advance_bcp(1);
            break;
        }

        case Bytecodes::_swap: {
            intptr_t val1 = frame->pop_raw();
            intptr_t val2 = frame->pop_raw();
            frame->push_raw(val1);
            frame->push_raw(val2);
            frame->advance_bcp(1);
            break;
        }

        // ================================================================
        // 整数算术
        // ================================================================

        case Bytecodes::_iadd: {
            jint v2 = frame->pop_int();
            jint v1 = frame->pop_int();
            frame->push_int(v1 + v2);
            frame->advance_bcp(1);
            break;
        }

        case Bytecodes::_isub: {
            jint v2 = frame->pop_int();
            jint v1 = frame->pop_int();
            frame->push_int(v1 - v2);
            frame->advance_bcp(1);
            break;
        }

        case Bytecodes::_imul: {
            jint v2 = frame->pop_int();
            jint v1 = frame->pop_int();
            frame->push_int(v1 * v2);
            frame->advance_bcp(1);
            break;
        }

        case Bytecodes::_idiv: {
            jint v2 = frame->pop_int();
            jint v1 = frame->pop_int();
            if (v2 == 0) {
                thread->set_pending_exception(nullptr, "java.lang.ArithmeticException: / by zero");
                return;
            }
            frame->push_int(v1 / v2);
            frame->advance_bcp(1);
            break;
        }

        case Bytecodes::_irem: {
            jint v2 = frame->pop_int();
            jint v1 = frame->pop_int();
            if (v2 == 0) {
                thread->set_pending_exception(nullptr, "java.lang.ArithmeticException: / by zero");
                return;
            }
            frame->push_int(v1 % v2);
            frame->advance_bcp(1);
            break;
        }

        case Bytecodes::_ineg: {
            jint v = frame->pop_int();
            frame->push_int(-v);
            frame->advance_bcp(1);
            break;
        }

        // ================================================================
        // 位运算
        // ================================================================

        case Bytecodes::_iand: {
            jint v2 = frame->pop_int();
            jint v1 = frame->pop_int();
            frame->push_int(v1 & v2);
            frame->advance_bcp(1);
            break;
        }

        case Bytecodes::_ior: {
            jint v2 = frame->pop_int();
            jint v1 = frame->pop_int();
            frame->push_int(v1 | v2);
            frame->advance_bcp(1);
            break;
        }

        case Bytecodes::_ixor: {
            jint v2 = frame->pop_int();
            jint v1 = frame->pop_int();
            frame->push_int(v1 ^ v2);
            frame->advance_bcp(1);
            break;
        }

        case Bytecodes::_ishl: {
            jint shift = frame->pop_int();
            jint v = frame->pop_int();
            frame->push_int(v << (shift & 0x1F));
            frame->advance_bcp(1);
            break;
        }

        case Bytecodes::_ishr: {
            jint shift = frame->pop_int();
            jint v = frame->pop_int();
            frame->push_int(v >> (shift & 0x1F));
            frame->advance_bcp(1);
            break;
        }

        case Bytecodes::_iushr: {
            jint shift = frame->pop_int();
            jint v = frame->pop_int();
            frame->push_int((jint)((juint)v >> (shift & 0x1F)));
            frame->advance_bcp(1);
            break;
        }

        // ================================================================
        // 自增
        // ================================================================

        case Bytecodes::_iinc: {
            int index = frame->read_u1_operand(1);
            jint delta = frame->read_s1_operand(2);
            jint val = (jint)frame->local_int(index);
            frame->set_local_int(index, val + delta);
            frame->advance_bcp(3);
            break;
        }

        // ================================================================
        // 类型转换（整数→其他）
        // ================================================================

        case Bytecodes::_i2l: {
            jint v = frame->pop_int();
            frame->push_long((jlong)v);
            frame->advance_bcp(1);
            break;
        }

        case Bytecodes::_i2b: {
            jint v = frame->pop_int();
            frame->push_int((jint)(jbyte)v);
            frame->advance_bcp(1);
            break;
        }

        case Bytecodes::_i2c: {
            jint v = frame->pop_int();
            frame->push_int((jint)(jchar)v);
            frame->advance_bcp(1);
            break;
        }

        case Bytecodes::_i2s: {
            jint v = frame->pop_int();
            frame->push_int((jint)(jshort)v);
            frame->advance_bcp(1);
            break;
        }

        // ================================================================
        // 条件跳转（与 0 比较）
        // ================================================================

        case Bytecodes::_ifeq: {
            jint v = frame->pop_int();
            if (v == 0) {
                int offset = frame->read_s2_operand(1);
                frame->advance_bcp(offset);
            } else {
                frame->advance_bcp(3);
            }
            break;
        }

        case Bytecodes::_ifne: {
            jint v = frame->pop_int();
            if (v != 0) {
                int offset = frame->read_s2_operand(1);
                frame->advance_bcp(offset);
            } else {
                frame->advance_bcp(3);
            }
            break;
        }

        case Bytecodes::_iflt: {
            jint v = frame->pop_int();
            if (v < 0) {
                int offset = frame->read_s2_operand(1);
                frame->advance_bcp(offset);
            } else {
                frame->advance_bcp(3);
            }
            break;
        }

        case Bytecodes::_ifge: {
            jint v = frame->pop_int();
            if (v >= 0) {
                int offset = frame->read_s2_operand(1);
                frame->advance_bcp(offset);
            } else {
                frame->advance_bcp(3);
            }
            break;
        }

        case Bytecodes::_ifgt: {
            jint v = frame->pop_int();
            if (v > 0) {
                int offset = frame->read_s2_operand(1);
                frame->advance_bcp(offset);
            } else {
                frame->advance_bcp(3);
            }
            break;
        }

        case Bytecodes::_ifle: {
            jint v = frame->pop_int();
            if (v <= 0) {
                int offset = frame->read_s2_operand(1);
                frame->advance_bcp(offset);
            } else {
                frame->advance_bcp(3);
            }
            break;
        }

        // ================================================================
        // 条件跳转（两个 int 比较）
        // ================================================================

        case Bytecodes::_if_icmpeq: {
            jint v2 = frame->pop_int();
            jint v1 = frame->pop_int();
            if (v1 == v2) {
                int offset = frame->read_s2_operand(1);
                frame->advance_bcp(offset);
            } else {
                frame->advance_bcp(3);
            }
            break;
        }

        case Bytecodes::_if_icmpne: {
            jint v2 = frame->pop_int();
            jint v1 = frame->pop_int();
            if (v1 != v2) {
                int offset = frame->read_s2_operand(1);
                frame->advance_bcp(offset);
            } else {
                frame->advance_bcp(3);
            }
            break;
        }

        case Bytecodes::_if_icmplt: {
            jint v2 = frame->pop_int();
            jint v1 = frame->pop_int();
            if (v1 < v2) {
                int offset = frame->read_s2_operand(1);
                frame->advance_bcp(offset);
            } else {
                frame->advance_bcp(3);
            }
            break;
        }

        case Bytecodes::_if_icmpge: {
            jint v2 = frame->pop_int();
            jint v1 = frame->pop_int();
            if (v1 >= v2) {
                int offset = frame->read_s2_operand(1);
                frame->advance_bcp(offset);
            } else {
                frame->advance_bcp(3);
            }
            break;
        }

        case Bytecodes::_if_icmpgt: {
            jint v2 = frame->pop_int();
            jint v1 = frame->pop_int();
            if (v1 > v2) {
                int offset = frame->read_s2_operand(1);
                frame->advance_bcp(offset);
            } else {
                frame->advance_bcp(3);
            }
            break;
        }

        case Bytecodes::_if_icmple: {
            jint v2 = frame->pop_int();
            jint v1 = frame->pop_int();
            if (v1 <= v2) {
                int offset = frame->read_s2_operand(1);
                frame->advance_bcp(offset);
            } else {
                frame->advance_bcp(3);
            }
            break;
        }

        // ================================================================
        // 无条件跳转
        // ================================================================

        case Bytecodes::_goto: {
            int offset = frame->read_s2_operand(1);
            frame->advance_bcp(offset);
            break;
        }

        // ================================================================
        // 方法返回
        // ================================================================

        case Bytecodes::_ireturn: {
            jint ret = frame->pop_int();
            if (result != nullptr) {
                result->set_jint(ret);
            }
            if (_trace_bytecodes) {
                fprintf(stderr, "  [RET] ireturn → %d\n", ret);
            }
            return;
        }

        case Bytecodes::_lreturn: {
            jlong ret = frame->pop_long();
            if (result != nullptr) {
                result->set_jlong(ret);
            }
            return;
        }

        case Bytecodes::_areturn: {
            oopDesc* ret = frame->pop_oop();
            if (result != nullptr) {
                result->set_oop(ret);
            }
            return;
        }

        case Bytecodes::_return: {
            if (result != nullptr) {
                result->set_type(T_VOID);
            }
            if (_trace_bytecodes) {
                fprintf(stderr, "  [RET] return (void)\n");
            }
            return;
        }

        // ================================================================
        // 方法调用
        // ================================================================

        case Bytecodes::_invokestatic: {
            u2 cp_index = (u2)frame->read_u2_operand(1);
            invoke_method(frame, klass, thread, cp_index, true);
            if (thread->has_pending_exception()) return;
            frame->advance_bcp(3);
            break;
        }

        case Bytecodes::_invokespecial: {
            u2 cp_index = (u2)frame->read_u2_operand(1);
            // invokespecial: 用于 <init>、private 方法、super 调用
            // 简化版：对 <init> 调用，弹出 receiver 后忽略（因为我们没有真正的对象创建）
            invoke_method(frame, klass, thread, cp_index, false);
            if (thread->has_pending_exception()) return;
            frame->advance_bcp(3);
            break;
        }

        case Bytecodes::_invokevirtual: {
            u2 cp_index = (u2)frame->read_u2_operand(1);
            handle_invokevirtual(frame, klass, thread, cp_index, result);
            if (thread->has_pending_exception()) return;
            frame->advance_bcp(3);
            break;
        }

        // ================================================================
        // 字段访问（Phase 5: 真实实现）
        // ================================================================

        case Bytecodes::_getstatic: {
            u2 cp_index = (u2)frame->read_u2_operand(1);
            handle_getstatic(frame, klass, cp_index);
            frame->advance_bcp(3);
            break;
        }

        case Bytecodes::_putstatic: {
            u2 cp_index = (u2)frame->read_u2_operand(1);
            handle_putstatic(frame, klass, cp_index);
            frame->advance_bcp(3);
            break;
        }

        case Bytecodes::_getfield: {
            u2 cp_index = (u2)frame->read_u2_operand(1);
            handle_getfield(frame, klass, cp_index);
            frame->advance_bcp(3);
            break;
        }

        case Bytecodes::_putfield: {
            u2 cp_index = (u2)frame->read_u2_operand(1);
            handle_putfield(frame, klass, thread, cp_index);
            frame->advance_bcp(3);
            break;
        }

        // ================================================================
        // 对象创建（Phase 5: 真实实现）
        // ================================================================

        case Bytecodes::_new: {
            u2 cp_index = (u2)frame->read_u2_operand(1);
            handle_new(frame, klass, thread, cp_index);
            if (thread->has_pending_exception()) return;
            frame->advance_bcp(3);
            break;
        }

        // ================================================================
        // 数组操作（Phase 7）
        // ================================================================

        case Bytecodes::_newarray: {
            // newarray atype
            // 操作数栈: ..., count → ..., arrayref
            u1 atype = frame->read_u1_operand(1);
            jint count = frame->pop_int();

            if (count < 0) {
                thread->set_pending_exception(nullptr,
                    "java.lang.NegativeArraySizeException");
                return;
            }

            TypeArrayKlass* tak = TypeArrayKlass::for_atype(atype);
            if (tak == nullptr) {
                fprintf(stderr, "ERROR: newarray — unknown atype %d\n", atype);
                thread->set_pending_exception(nullptr, "Unknown array type");
                return;
            }

            typeArrayOopDesc* arr = tak->allocate_array(count);
            if (arr == nullptr) {
                thread->set_pending_exception(nullptr,
                    "java.lang.OutOfMemoryError: Java heap space");
                return;
            }

            if (_trace_bytecodes) {
                fprintf(stderr, "  [NEWARRAY] atype=%d, count=%d, %s at %p (%d bytes)\n",
                        atype, count, tak->name(), (void*)arr,
                        tak->array_size_in_bytes(count));
            }

            frame->push_oop((oopDesc*)arr);
            frame->advance_bcp(2);
            break;
        }

        case Bytecodes::_arraylength: {
            // arraylength
            // 操作数栈: ..., arrayref → ..., length
            oopDesc* arr_oop = frame->pop_oop();
            if (arr_oop == nullptr) {
                thread->set_pending_exception(nullptr,
                    "java.lang.NullPointerException");
                return;
            }
            arrayOopDesc* arr = (arrayOopDesc*)arr_oop;
            frame->push_int(arr->length());
            frame->advance_bcp(1);
            break;
        }

        case Bytecodes::_iaload: {
            // iaload: ..., arrayref, index → ..., value
            jint index = frame->pop_int();
            typeArrayOopDesc* arr = (typeArrayOopDesc*)frame->pop_oop();
            if (arr == nullptr) {
                thread->set_pending_exception(nullptr, "java.lang.NullPointerException");
                return;
            }
            if (index < 0 || index >= arr->length()) {
                fprintf(stderr, "ERROR: ArrayIndexOutOfBoundsException: index=%d, length=%d\n",
                        index, arr->length());
                thread->set_pending_exception(nullptr,
                    "java.lang.ArrayIndexOutOfBoundsException");
                return;
            }
            frame->push_int(arr->int_at(index));
            frame->advance_bcp(1);
            break;
        }

        case Bytecodes::_iastore: {
            // iastore: ..., arrayref, index, value → ...
            jint value = frame->pop_int();
            jint index = frame->pop_int();
            typeArrayOopDesc* arr = (typeArrayOopDesc*)frame->pop_oop();
            if (arr == nullptr) {
                thread->set_pending_exception(nullptr, "java.lang.NullPointerException");
                return;
            }
            if (index < 0 || index >= arr->length()) {
                fprintf(stderr, "ERROR: ArrayIndexOutOfBoundsException: index=%d, length=%d\n",
                        index, arr->length());
                thread->set_pending_exception(nullptr,
                    "java.lang.ArrayIndexOutOfBoundsException");
                return;
            }
            arr->int_at_put(index, value);
            frame->advance_bcp(1);
            break;
        }

        case Bytecodes::_baload: {
            // baload: ..., arrayref, index → ..., value (byte/boolean)
            jint index = frame->pop_int();
            typeArrayOopDesc* arr = (typeArrayOopDesc*)frame->pop_oop();
            if (arr == nullptr) {
                thread->set_pending_exception(nullptr, "java.lang.NullPointerException");
                return;
            }
            if (index < 0 || index >= arr->length()) {
                thread->set_pending_exception(nullptr,
                    "java.lang.ArrayIndexOutOfBoundsException");
                return;
            }
            frame->push_int((jint)arr->byte_at(index));
            frame->advance_bcp(1);
            break;
        }

        case Bytecodes::_bastore: {
            // bastore: ..., arrayref, index, value → ...
            jint value = frame->pop_int();
            jint index = frame->pop_int();
            typeArrayOopDesc* arr = (typeArrayOopDesc*)frame->pop_oop();
            if (arr == nullptr) {
                thread->set_pending_exception(nullptr, "java.lang.NullPointerException");
                return;
            }
            if (index < 0 || index >= arr->length()) {
                thread->set_pending_exception(nullptr,
                    "java.lang.ArrayIndexOutOfBoundsException");
                return;
            }
            arr->byte_at_put(index, (jbyte)value);
            frame->advance_bcp(1);
            break;
        }

        case Bytecodes::_caload: {
            // caload: ..., arrayref, index → ..., value (char)
            jint index = frame->pop_int();
            typeArrayOopDesc* arr = (typeArrayOopDesc*)frame->pop_oop();
            if (arr == nullptr) {
                thread->set_pending_exception(nullptr, "java.lang.NullPointerException");
                return;
            }
            if (index < 0 || index >= arr->length()) {
                thread->set_pending_exception(nullptr,
                    "java.lang.ArrayIndexOutOfBoundsException");
                return;
            }
            frame->push_int((jint)arr->char_at(index));
            frame->advance_bcp(1);
            break;
        }

        case Bytecodes::_castore: {
            // castore: ..., arrayref, index, value → ...
            jint value = frame->pop_int();
            jint index = frame->pop_int();
            typeArrayOopDesc* arr = (typeArrayOopDesc*)frame->pop_oop();
            if (arr == nullptr) {
                thread->set_pending_exception(nullptr, "java.lang.NullPointerException");
                return;
            }
            if (index < 0 || index >= arr->length()) {
                thread->set_pending_exception(nullptr,
                    "java.lang.ArrayIndexOutOfBoundsException");
                return;
            }
            arr->char_at_put(index, (jchar)value);
            frame->advance_bcp(1);
            break;
        }

        case Bytecodes::_saload: {
            // saload: ..., arrayref, index → ..., value (short)
            jint index = frame->pop_int();
            typeArrayOopDesc* arr = (typeArrayOopDesc*)frame->pop_oop();
            if (arr == nullptr) {
                thread->set_pending_exception(nullptr, "java.lang.NullPointerException");
                return;
            }
            if (index < 0 || index >= arr->length()) {
                thread->set_pending_exception(nullptr,
                    "java.lang.ArrayIndexOutOfBoundsException");
                return;
            }
            frame->push_int((jint)arr->short_at(index));
            frame->advance_bcp(1);
            break;
        }

        case Bytecodes::_sastore: {
            // sastore: ..., arrayref, index, value → ...
            jint value = frame->pop_int();
            jint index = frame->pop_int();
            typeArrayOopDesc* arr = (typeArrayOopDesc*)frame->pop_oop();
            if (arr == nullptr) {
                thread->set_pending_exception(nullptr, "java.lang.NullPointerException");
                return;
            }
            if (index < 0 || index >= arr->length()) {
                thread->set_pending_exception(nullptr,
                    "java.lang.ArrayIndexOutOfBoundsException");
                return;
            }
            arr->short_at_put(index, (jshort)value);
            frame->advance_bcp(1);
            break;
        }

        // ================================================================
        // 未实现的字节码
        // ================================================================

        default:
            fprintf(stderr, "ERROR: Unimplemented bytecode 0x%02X (%s) at bci=%d\n",
                    opcode,
                    Bytecodes::name((Bytecodes::Code)opcode),
                    frame->bci());
            thread->set_pending_exception(nullptr, "Unimplemented bytecode");
            return;

        } // end switch
    } // end while
}

// ============================================================================
// invoke_method — 处理 invokestatic / invokespecial
//
// 流程：
//   1. 从常量池解析 Methodref → 找到类名+方法名+签名
//   2. 在当前 klass 中查找方法（简化：只查当前类）
//   3. 创建新栈帧，设置参数
//   4. 递归执行
//   5. 将返回值压回调用者栈
// ============================================================================

void BytecodeInterpreter::invoke_method(InterpreterFrame* frame,
                                        InstanceKlass* klass,
                                        JavaThread* thread,
                                        u2 cp_index,
                                        bool is_static)
{
    ConstantPool* cp = klass->constants();

    // 解析 Methodref: class_index + name_and_type_index
    jbyte tag = cp->tag_at(cp_index).value();
    if (tag != JVM_CONSTANT_Methodref && tag != JVM_CONSTANT_InterfaceMethodref) {
        fprintf(stderr, "ERROR: invoke cp#%d is not Methodref (tag=%d)\n", cp_index, tag);
        thread->set_pending_exception(nullptr, "Not a Methodref");
        return;
    }

    // 获取 class_index 和 name_and_type_index
    // 打包格式: (nat_index << 16) | class_index
    u2 class_index = cp->unchecked_klass_ref_index_at(cp_index);
    u2 nat_index = cp->unchecked_name_and_type_ref_index_at(cp_index);

    // 解析 NameAndType
    u2 name_index = (u2)cp->name_ref_index_at(nat_index);
    u2 sig_index = (u2)cp->signature_ref_index_at(nat_index);

    const char* method_name = cp->utf8_at(name_index);
    const char* method_sig = cp->utf8_at(sig_index);

    if (_trace_bytecodes) {
        // 解析类名
        const char* class_name = cp->klass_name_at(class_index);
        fprintf(stderr, "  [INVOKE] %s.%s%s (cp#%d)\n",
                class_name, method_name, method_sig, cp_index);
    }

    // 检查目标类是否是当前类
    // 简化版：只在当前类中执行方法，其他类的方法做桩处理
    const char* target_class_name = cp->klass_name_at(class_index);
    bool is_same_class = (strcmp(target_class_name, klass->class_name()) == 0);

    if (!is_same_class) {
        // 目标类不是当前类 → 无法执行
        // 对 <init> 调用，弹出 receiver 后忽略
        if (strcmp(method_name, "<init>") == 0 && !is_static) {
            frame->pop_raw();  // 弹出 receiver
            if (_trace_bytecodes) {
                fprintf(stderr, "  [INVOKE] %s.<init> — skipped (external class)\n",
                        target_class_name);
            }
            return;
        }
        // 其他外部方法：根据签名弹出参数和 receiver
        if (_trace_bytecodes) {
            fprintf(stderr, "  [INVOKE] %s.%s%s — skipped (external class)\n",
                    target_class_name, method_name, method_sig);
        }
        // 计算需要弹出的参数数量
        int param_slots_to_pop = 0;
        const char* sig_scan = method_sig;
        if (*sig_scan == '(') sig_scan++;
        while (*sig_scan != ')' && *sig_scan != '\0') {
            if (*sig_scan == 'J' || *sig_scan == 'D') {
                param_slots_to_pop += 2; sig_scan++;
            } else if (*sig_scan == 'L') {
                param_slots_to_pop += 1;
                while (*sig_scan != ';' && *sig_scan != '\0') sig_scan++;
                if (*sig_scan == ';') sig_scan++;
            } else if (*sig_scan == '[') {
                param_slots_to_pop += 1; sig_scan++;
                if (*sig_scan == 'L') {
                    while (*sig_scan != ';' && *sig_scan != '\0') sig_scan++;
                    if (*sig_scan == ';') sig_scan++;
                } else { sig_scan++; }
            } else {
                param_slots_to_pop += 1; sig_scan++;
            }
        }
        if (!is_static) param_slots_to_pop += 1; // receiver
        for (int i = 0; i < param_slots_to_pop; i++) frame->pop_raw();
        return;
    }

    // 查找方法（在当前类中查找）
    Method* target = klass->find_method(method_name, method_sig);

    if (target == nullptr) {
        // 方法不在当前类中 → 需要弹出参数保持栈平衡
        if (_trace_bytecodes) {
            fprintf(stderr, "  [INVOKE] %s%s not found in %s — skipping\n",
                    method_name, method_sig, klass->class_name());
        }
        // 计算需要弹出的参数数量
        int skip_slots = 0;
        const char* skip_sig = method_sig;
        if (*skip_sig == '(') skip_sig++;
        while (*skip_sig != ')' && *skip_sig != '\0') {
            if (*skip_sig == 'J' || *skip_sig == 'D') {
                skip_slots += 2; skip_sig++;
            } else if (*skip_sig == 'L') {
                skip_slots += 1;
                while (*skip_sig != ';' && *skip_sig != '\0') skip_sig++;
                if (*skip_sig == ';') skip_sig++;
            } else if (*skip_sig == '[') {
                skip_slots += 1; skip_sig++;
                if (*skip_sig == 'L') {
                    while (*skip_sig != ';' && *skip_sig != '\0') skip_sig++;
                    if (*skip_sig == ';') skip_sig++;
                } else if (*skip_sig != '\0') { skip_sig++; }
            } else {
                skip_slots += 1; skip_sig++;
            }
        }
        if (!is_static) skip_slots += 1; // receiver
        for (int i = 0; i < skip_slots; i++) frame->pop_raw();
        return;
    }

    // 计算参数个数（从签名解析）
    int param_slots = 0;
    const char* sig = method_sig;
    if (*sig == '(') sig++;
    while (*sig != ')' && *sig != '\0') {
        if (*sig == 'J' || *sig == 'D') {
            param_slots += 2;  // long/double 占 2 slot
            sig++;
        } else if (*sig == 'L') {
            param_slots += 1;
            while (*sig != ';' && *sig != '\0') sig++;
            if (*sig == ';') sig++;
        } else if (*sig == '[') {
            param_slots += 1;
            sig++;
            if (*sig == 'L') {
                while (*sig != ';' && *sig != '\0') sig++;
                if (*sig == ';') sig++;
            } else {
                sig++;
            }
        } else {
            param_slots += 1;
            sig++;
        }
    }

    // 非 static 方法还有 receiver
    int total_args = is_static ? param_slots : param_slots + 1;

    // 从操作数栈弹出参数（倒序 → 正序到局部变量表）
    intptr_t* args = NEW_C_HEAP_ARRAY(intptr_t, total_args, mtThread);
    for (int i = total_args - 1; i >= 0; i--) {
        args[i] = frame->pop_raw();
    }

    // 创建新帧并执行
    JavaValue call_result(T_VOID);
    InterpreterFrame callee_frame(target, klass->constants(), frame);

    // 设置参数到被调方法的局部变量表
    for (int i = 0; i < total_args && i < target->max_locals(); i++) {
        callee_frame.set_local_int(i, args[i]);
    }

    FREE_C_HEAP_ARRAY(intptr_t, args);

    // 递归执行
    run(&callee_frame, klass, thread, &call_result);

    // 如果被调方法有返回值，压入调用者栈
    if (!thread->has_pending_exception()) {
        // 解析返回类型
        const char* ret_sig = strchr(method_sig, ')');
        if (ret_sig != nullptr) {
            ret_sig++;  // 跳过 ')'
            switch (*ret_sig) {
                case 'V': break; // void，不压栈
                case 'I': case 'Z': case 'B': case 'C': case 'S':
                    frame->push_int(call_result.get_jint());
                    break;
                case 'J':
                    frame->push_long(call_result.get_jlong());
                    break;
                case 'F':
                    frame->push_float(call_result.get_jfloat());
                    break;
                case 'D':
                    frame->push_double(call_result.get_jdouble());
                    break;
                case 'L': case '[':
                    frame->push_oop(call_result.get_oop());
                    break;
                default:
                    break;
            }
        }
    }
}

// ============================================================================
// resolve_field_type — 解析 Fieldref 的字段类型
// 返回描述符的第一个字符，用于确定字段大小
// ============================================================================

char BytecodeInterpreter::resolve_field_type(ConstantPool* cp, u2 cp_index) {
    u2 nat_index = cp->unchecked_name_and_type_ref_index_at(cp_index);
    u2 desc_index = (u2)cp->signature_ref_index_at(nat_index);
    const char* desc = cp->utf8_at(desc_index);
    return desc[0];
}

// ============================================================================
// handle_getstatic — 处理 getstatic
//
// Phase 5: 真实静态字段访问
//   1. 解析 Fieldref → 类名.字段名:描述符
//   2. 如果是当前类的字段 → 从 _static_fields 数组读取
//   3. 如果是外部类（如 System.out）→ 保留桩处理
// ============================================================================

void BytecodeInterpreter::handle_getstatic(InterpreterFrame* frame,
                                            InstanceKlass* klass,
                                            u2 cp_index)
{
    ConstantPool* cp = klass->constants();

    // 解析 Fieldref → 类名.字段名:描述符
    u2 class_index = cp->unchecked_klass_ref_index_at(cp_index);
    u2 nat_index = cp->unchecked_name_and_type_ref_index_at(cp_index);

    u2 name_index = (u2)cp->name_ref_index_at(nat_index);
    u2 desc_index = (u2)cp->signature_ref_index_at(nat_index);

    const char* field_name = cp->utf8_at(name_index);
    const char* field_desc = cp->utf8_at(desc_index);
    const char* class_name = cp->klass_name_at(class_index);

    if (_trace_bytecodes) {
        fprintf(stderr, "  [GETSTATIC] %s.%s:%s (cp#%d)\n",
                class_name, field_name, field_desc, cp_index);
    }

    // 判断是否是当前类的字段
    bool is_same_class = (strcmp(class_name, klass->class_name()) == 0);

    if (is_same_class) {
        // 当前类的静态字段 → 从 _static_fields 数组读取
        int idx = klass->static_field_index(field_name);
        if (idx >= 0) {
            intptr_t value = klass->static_field_value(idx);
            // 根据描述符类型压栈
            switch (field_desc[0]) {
                case 'J':  // long
                    frame->push_long((jlong)value);
                    break;
                case 'D': { // double
                    jdouble dval;
                    memcpy(&dval, &value, sizeof(jdouble));
                    frame->push_double(dval);
                    break;
                }
                case 'L': case '[':  // reference
                    frame->push_oop((oopDesc*)value);
                    break;
                default:  // int, short, byte, char, boolean, float
                    frame->push_int((jint)value);
                    break;
            }
            return;
        }
    }

    // 外部类的静态字段：保留桩处理
    // System.out → PrintStream 标记值
    if (strcmp(field_name, "out") == 0 &&
        strcmp(class_name, "java/lang/System") == 0) {
        frame->push_oop((oopDesc*)(intptr_t)0xDEAD0002);  // PrintStream 标记
    } else {
        // 其他外部静态字段默认值
        if (field_desc[0] == 'J' || field_desc[0] == 'D') {
            frame->push_long(0);  // long/double 默认 0
        } else {
            frame->push_int(0);
        }
    }
}

// ============================================================================
// handle_putstatic — 处理 putstatic
//
// Phase 5: 真实静态字段写入
//   1. 解析 Fieldref
//   2. 如果是当前类 → 写入 _static_fields 数组
//   3. 如果是外部类 → 弹出值丢弃
// ============================================================================

void BytecodeInterpreter::handle_putstatic(InterpreterFrame* frame,
                                            InstanceKlass* klass,
                                            u2 cp_index)
{
    ConstantPool* cp = klass->constants();

    u2 class_index = cp->unchecked_klass_ref_index_at(cp_index);
    u2 nat_index = cp->unchecked_name_and_type_ref_index_at(cp_index);

    u2 name_index = (u2)cp->name_ref_index_at(nat_index);
    u2 desc_index = (u2)cp->signature_ref_index_at(nat_index);

    const char* field_name = cp->utf8_at(name_index);
    const char* field_desc = cp->utf8_at(desc_index);
    const char* class_name = cp->klass_name_at(class_index);

    if (_trace_bytecodes) {
        fprintf(stderr, "  [PUTSTATIC] %s.%s:%s (cp#%d)\n",
                class_name, field_name, field_desc, cp_index);
    }

    bool is_same_class = (strcmp(class_name, klass->class_name()) == 0);

    if (is_same_class) {
        int idx = klass->static_field_index(field_name);
        if (idx >= 0) {
            // 根据类型弹出值
            intptr_t value;
            switch (field_desc[0]) {
                case 'J':  // long
                    value = (intptr_t)frame->pop_long();
                    break;
                case 'D': { // double
                    jdouble dval = frame->pop_double();
                    memcpy(&value, &dval, sizeof(jdouble));
                    break;
                }
                case 'L': case '[':  // reference
                    value = (intptr_t)frame->pop_oop();
                    break;
                default:  // int, short, byte, char, boolean, float
                    value = (intptr_t)frame->pop_int();
                    break;
            }
            klass->set_static_field_value(idx, value);
            return;
        }
    }

    // 外部类：弹出值丢弃
    if (field_desc[0] == 'J' || field_desc[0] == 'D') {
        frame->pop_long();
    } else {
        frame->pop_raw();
    }
}

// ============================================================================
// handle_getfield — 处理 getfield
//
// Phase 5: 真实实例字段读取
//   1. 解析 Fieldref → 字段名 + 描述符
//   2. 在当前类的字段表中查找该字段
//   3. 弹出 objectref
//   4. 根据字段偏移和类型从对象中读取值并压栈
// ============================================================================

void BytecodeInterpreter::handle_getfield(InterpreterFrame* frame,
                                           InstanceKlass* klass,
                                           u2 cp_index)
{
    ConstantPool* cp = klass->constants();

    u2 nat_index = cp->unchecked_name_and_type_ref_index_at(cp_index);
    u2 name_index = (u2)cp->name_ref_index_at(nat_index);
    u2 desc_index = (u2)cp->signature_ref_index_at(nat_index);

    const char* field_name = cp->utf8_at(name_index);
    const char* field_desc = cp->utf8_at(desc_index);

    if (_trace_bytecodes) {
        u2 class_index = cp->unchecked_klass_ref_index_at(cp_index);
        const char* class_name = cp->klass_name_at(class_index);
        fprintf(stderr, "  [GETFIELD] %s.%s:%s (cp#%d)\n",
                class_name, field_name, field_desc, cp_index);
    }

    // 弹出 objectref
    oopDesc* obj = frame->pop_oop();
    if (obj == nullptr) {
        fprintf(stderr, "ERROR: NullPointerException at getfield %s\n", field_name);
        frame->push_int(0);
        return;
    }

    // 在当前类中查找字段信息获取偏移
    const FieldInfoEntry* field = klass->find_field(field_name);
    if (field == nullptr || field->is_static()) {
        fprintf(stderr, "WARNING: getfield — field %s not found or is static\n", field_name);
        frame->push_int(0);
        return;
    }

    int offset = field->offset;

    // 根据描述符类型读取并压栈
    switch (field_desc[0]) {
        case 'I':  // int
            frame->push_int(obj->int_field(offset));
            break;
        case 'J':  // long
            frame->push_long(obj->long_field(offset));
            break;
        case 'F':  // float
            frame->push_float(obj->float_field(offset));
            break;
        case 'D':  // double
            frame->push_double(obj->double_field(offset));
            break;
        case 'B':  // byte
            frame->push_int((jint)obj->byte_field(offset));
            break;
        case 'Z':  // boolean
            frame->push_int((jint)obj->bool_field(offset));
            break;
        case 'S':  // short
            frame->push_int((jint)obj->short_field(offset));
            break;
        case 'C':  // char
            frame->push_int((jint)obj->char_field(offset));
            break;
        case 'L': case '[':  // reference
            frame->push_oop(obj->obj_field(offset));
            break;
        default:
            fprintf(stderr, "WARNING: getfield — unknown descriptor '%c'\n", field_desc[0]);
            frame->push_int(0);
            break;
    }
}

// ============================================================================
// handle_putfield — 处理 putfield
//
// Phase 5: 真实实例字段写入
//   1. 解析 Fieldref → 字段名 + 描述符
//   2. 弹出 value 和 objectref
//   3. 根据字段偏移和类型写入对象
// ============================================================================

void BytecodeInterpreter::handle_putfield(InterpreterFrame* frame,
                                           InstanceKlass* klass,
                                           JavaThread* thread,
                                           u2 cp_index)
{
    ConstantPool* cp = klass->constants();

    u2 nat_index = cp->unchecked_name_and_type_ref_index_at(cp_index);
    u2 name_index = (u2)cp->name_ref_index_at(nat_index);
    u2 desc_index = (u2)cp->signature_ref_index_at(nat_index);

    const char* field_name = cp->utf8_at(name_index);
    const char* field_desc = cp->utf8_at(desc_index);

    if (_trace_bytecodes) {
        u2 class_index = cp->unchecked_klass_ref_index_at(cp_index);
        const char* class_name = cp->klass_name_at(class_index);
        fprintf(stderr, "  [PUTFIELD] %s.%s:%s (cp#%d)\n",
                class_name, field_name, field_desc, cp_index);
    }

    // 在当前类中查找字段
    const FieldInfoEntry* field = klass->find_field(field_name);
    if (field == nullptr || field->is_static()) {
        fprintf(stderr, "WARNING: putfield — field %s not found or is static\n", field_name);
        // 需要弹出 value 和 objectref
        if (field_desc[0] == 'J' || field_desc[0] == 'D') {
            frame->pop_long();  // value (2 slots)
        } else {
            frame->pop_raw();   // value (1 slot)
        }
        frame->pop_raw();  // objectref
        return;
    }

    int offset = field->offset;

    // 根据描述符类型弹出值并写入
    switch (field_desc[0]) {
        case 'I': {
            jint value = frame->pop_int();
            oopDesc* obj = frame->pop_oop();
            if (obj == nullptr) {
                fprintf(stderr, "ERROR: NullPointerException at putfield %s\n", field_name);
                return;
            }
            obj->int_field_put(offset, value);
            break;
        }
        case 'J': {
            jlong value = frame->pop_long();
            oopDesc* obj = frame->pop_oop();
            if (obj == nullptr) {
                fprintf(stderr, "ERROR: NullPointerException at putfield %s\n", field_name);
                return;
            }
            obj->long_field_put(offset, value);
            break;
        }
        case 'F': {
            jfloat value = frame->pop_float();
            oopDesc* obj = frame->pop_oop();
            if (obj == nullptr) {
                fprintf(stderr, "ERROR: NullPointerException at putfield %s\n", field_name);
                return;
            }
            obj->float_field_put(offset, value);
            break;
        }
        case 'D': {
            jdouble value = frame->pop_double();
            oopDesc* obj = frame->pop_oop();
            if (obj == nullptr) {
                fprintf(stderr, "ERROR: NullPointerException at putfield %s\n", field_name);
                return;
            }
            obj->double_field_put(offset, value);
            break;
        }
        case 'B': {
            jint value = frame->pop_int();
            oopDesc* obj = frame->pop_oop();
            if (obj == nullptr) {
                fprintf(stderr, "ERROR: NullPointerException at putfield %s\n", field_name);
                return;
            }
            obj->byte_field_put(offset, (jbyte)value);
            break;
        }
        case 'Z': {
            jint value = frame->pop_int();
            oopDesc* obj = frame->pop_oop();
            if (obj == nullptr) {
                fprintf(stderr, "ERROR: NullPointerException at putfield %s\n", field_name);
                return;
            }
            obj->bool_field_put(offset, (jboolean)value);
            break;
        }
        case 'S': {
            jint value = frame->pop_int();
            oopDesc* obj = frame->pop_oop();
            if (obj == nullptr) {
                fprintf(stderr, "ERROR: NullPointerException at putfield %s\n", field_name);
                return;
            }
            obj->short_field_put(offset, (jshort)value);
            break;
        }
        case 'C': {
            jint value = frame->pop_int();
            oopDesc* obj = frame->pop_oop();
            if (obj == nullptr) {
                fprintf(stderr, "ERROR: NullPointerException at putfield %s\n", field_name);
                return;
            }
            obj->char_field_put(offset, (jchar)value);
            break;
        }
        case 'L': case '[': {
            oopDesc* value = frame->pop_oop();
            oopDesc* obj = frame->pop_oop();
            if (obj == nullptr) {
                fprintf(stderr, "ERROR: NullPointerException at putfield %s\n", field_name);
                return;
            }
            obj->obj_field_put(offset, value);
            break;
        }
        default: {
            fprintf(stderr, "WARNING: putfield — unknown descriptor '%c'\n", field_desc[0]);
            frame->pop_raw();  // value
            frame->pop_raw();  // objectref
            break;
        }
    }
}

// ============================================================================
// handle_new — 处理 new 字节码
//
// Phase 5: 真实对象创建
//
// 对应 HotSpot 的执行链：
//   _new bytecode → InterpreterRuntime::_new()
//   → InstanceKlass::allocate_instance()
//   → CollectedHeap::obj_allocate()
//   → bump-pointer / TLAB
//
// 我们的简化版：
//   1. 从常量池解析 Class 索引 → 类名
//   2. 简化：只支持当前类（没有类加载器）
//   3. 调用 klass->allocate_instance() 在 JavaHeap 上分配
//   4. 压入 objectref
// ============================================================================

void BytecodeInterpreter::handle_new(InterpreterFrame* frame,
                                      InstanceKlass* klass,
                                      JavaThread* thread,
                                      u2 cp_index)
{
    ConstantPool* cp = klass->constants();

    // 解析 Class 索引 → 类名
    const char* class_name = cp->klass_name_at(cp_index);

    if (_trace_bytecodes) {
        fprintf(stderr, "  [NEW] %s (cp#%d)\n", class_name, cp_index);
    }

    // 简化：只支持创建当前类的实例
    // 真实 JVM 需要通过 SystemDictionary 查找/加载目标类
    bool is_same_class = (strcmp(class_name, klass->class_name()) == 0);

    if (!is_same_class) {
        // 外部类：无法创建，压入标记值（保持向后兼容）
        if (_trace_bytecodes) {
            fprintf(stderr, "  [NEW] %s — external class, using marker\n", class_name);
        }
        frame->push_oop((oopDesc*)(intptr_t)0xDEAD0001);
        return;
    }

    // 从 JavaHeap 分配对象
    oopDesc* obj = klass->allocate_instance();
    if (obj == nullptr) {
        thread->set_pending_exception(nullptr, "java.lang.OutOfMemoryError: Java heap space");
        return;
    }

    if (_trace_bytecodes) {
        fprintf(stderr, "  [NEW] Allocated %s at %p (%d bytes)\n",
                class_name, (void*)obj, klass->instance_size());
    }

    frame->push_oop(obj);
}

// ============================================================================
// handle_invokevirtual — 处理 invokevirtual
//
// 特殊处理：
//   - PrintStream.println(I)V → 实际打印整数到 stdout
//   - 其他 → 当作普通方法调用
// ============================================================================

void BytecodeInterpreter::handle_invokevirtual(InterpreterFrame* frame,
                                                InstanceKlass* klass,
                                                JavaThread* thread,
                                                u2 cp_index,
                                                JavaValue* result)
{
    ConstantPool* cp = klass->constants();

    // 解析 Methodref
    u2 class_index = cp->unchecked_klass_ref_index_at(cp_index);
    u2 nat_index = cp->unchecked_name_and_type_ref_index_at(cp_index);

    u2 name_index = (u2)cp->name_ref_index_at(nat_index);
    u2 sig_index = (u2)cp->signature_ref_index_at(nat_index);

    const char* method_name = cp->utf8_at(name_index);
    const char* method_sig = cp->utf8_at(sig_index);

    // 解析类名
    const char* class_name = cp->klass_name_at(class_index);

    if (_trace_bytecodes) {
        fprintf(stderr, "  [INVOKEVIRTUAL] %s.%s%s (cp#%d)\n",
                class_name, method_name, method_sig, cp_index);
    }

    // 特殊处理：PrintStream.println
    if (strcmp(class_name, "java/io/PrintStream") == 0 &&
        strcmp(method_name, "println") == 0)
    {
        if (strcmp(method_sig, "(I)V") == 0) {
            // println(int) — 弹出参数和 receiver，打印到 stdout
            jint value = frame->pop_int();
            frame->pop_oop();  // receiver (PrintStream)
            printf("%d\n", value);
            return;
        } else if (strcmp(method_sig, "(Ljava/lang/String;)V") == 0) {
            // println(String) — 弹出参数和 receiver，打印提示
            frame->pop_oop();  // String arg
            frame->pop_oop();  // receiver
            printf("[String output not yet supported]\n");
            return;
        } else if (strcmp(method_sig, "()V") == 0) {
            // println() — 空行
            frame->pop_oop();  // receiver
            printf("\n");
            return;
        }
    }

    // 普通 invokevirtual → 在当前类查找并执行
    invoke_method(frame, klass, thread, cp_index, false);
}
