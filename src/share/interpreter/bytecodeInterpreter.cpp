// ============================================================================
// Mini JVM - C++ 字节码解释器实现
// 对应 HotSpot: src/hotspot/share/interpreter/bytecodeInterpreter.cpp
//
// 使用 switch-case 派发，逐条执行字节码。
// HotSpot 的 C++ 解释器在编译时可选择 computed goto 优化，
// 我们使用标准 switch-case，清晰易懂。
// ============================================================================

#include "interpreter/bytecodeInterpreter.hpp"

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
        // 字段访问（桩实现）
        // ================================================================

        case Bytecodes::_getstatic: {
            u2 cp_index = (u2)frame->read_u2_operand(1);
            handle_getstatic(frame, klass, cp_index);
            frame->advance_bcp(3);
            break;
        }

        case Bytecodes::_putstatic: {
            // 简化：弹出值并丢弃
            frame->pop_raw();
            frame->advance_bcp(3);
            break;
        }

        case Bytecodes::_getfield: {
            // 简化版：弹出 objectref，压入 0
            frame->pop_oop();
            frame->push_int(0);
            frame->advance_bcp(3);
            break;
        }

        case Bytecodes::_putfield: {
            // 简化版：弹出 value 和 objectref
            frame->pop_raw();  // value
            frame->pop_raw();  // objectref
            frame->advance_bcp(3);
            break;
        }

        // ================================================================
        // 对象创建（桩实现）
        // ================================================================

        case Bytecodes::_new: {
            // 简化：压入一个非空标记值作为 objectref
            // 真正的对象创建需要 GC 和堆分配，后续实现
            frame->push_oop((oopDesc*)(intptr_t)0xDEAD0001);
            frame->advance_bcp(3);
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
        // 方法不在当前类中
        fprintf(stderr, "WARNING: Method %s%s not found in %s\n",
                method_name, method_sig, klass->class_name());
        // 不设异常，继续执行（宽容模式）
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
// handle_getstatic — 处理 getstatic（简化桩）
//
// 对于 System.out 等标准库字段，压入一个标记值。
// 后续实现完整的字段解析后替换。
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

    if (_trace_bytecodes) {
        const char* class_name = cp->klass_name_at(class_index);
        const char* field_desc = cp->utf8_at(desc_index);
        fprintf(stderr, "  [GETSTATIC] %s.%s:%s (cp#%d)\n",
                class_name, field_name, field_desc, cp_index);
    }

    // 对 System.out 压入一个标记值（模拟 PrintStream 对象引用）
    if (strcmp(field_name, "out") == 0) {
        frame->push_oop((oopDesc*)(intptr_t)0xDEAD0002);  // PrintStream 标记
    } else {
        frame->push_int(0);  // 其他静态字段默认 0
    }
}

// ============================================================================
// handle_invokevirtual — 处理 invokevirtual（简化桩）
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

// ============================================================================
// 辅助：打印常量池中 class 的名称
// ============================================================================
