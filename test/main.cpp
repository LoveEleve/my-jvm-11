// ============================================================================
// Mini JVM - Phase 8 测试入口
// 测试 OOP-Klass 对象模型 + 字节码解释器 + 对象创建 + 字段访问
// + 真实 <init> 构造函数 + 实例方法调用 + 数组支持
// + 引用比较 + Switch + Long 操作
// ============================================================================

#include "utilities/globalDefinitions.hpp"
#include "utilities/debug.hpp"
#include "utilities/macros.hpp"
#include "utilities/bytes.hpp"
#include "utilities/accessFlags.hpp"
#include "memory/allocation.hpp"
#include "oops/oopsHierarchy.hpp"
#include "oops/markOop.hpp"
#include "oops/oop.hpp"
#include "oops/instanceOop.hpp"
#include "oops/metadata.hpp"
#include "oops/constMethod.hpp"
#include "oops/method.hpp"
#include "oops/klass.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/arrayOop.hpp"
#include "oops/typeArrayOop.hpp"
#include "oops/typeArrayKlass.hpp"
#include "classfile/classFileStream.hpp"
#include "classfile/classFileParser.hpp"
#include "interpreter/bytecodes.hpp"
#include "runtime/javaThread.hpp"
#include "runtime/frame.hpp"
#include "interpreter/bytecodeInterpreter.hpp"
#include "gc/shared/javaHeap.hpp"

#include <cstdio>
#include <cstdlib>

// ----------------------------------------------------------------------------
// 读取文件到内存
// ----------------------------------------------------------------------------
static u1* read_class_file(const char* path, int* out_length) {
    FILE* f = fopen(path, "rb");
    if (f == nullptr) {
        fprintf(stderr, "Error: Cannot open file: %s\n", path);
        return nullptr;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    u1* buffer = NEW_C_HEAP_ARRAY(u1, size, mtClass);
    size_t rd = fread(buffer, 1, size, f);
    fclose(f);

    if ((long)rd != size) {
        FREE_C_HEAP_ARRAY(u1, buffer);
        return nullptr;
    }
    *out_length = (int)size;
    return buffer;
}

// ============================================================================
// Phase 1 测试：基础类型和工具
// ============================================================================

void test_bytes() {
    printf("=== Test: Bytes (Endian) ===\n");
    u1 data_u2[] = { 0x00, 0x37 };
    u2 val_u2 = Bytes::get_Java_u2((address)data_u2);
    vm_assert(val_u2 == 55, "u2 byte swap failed");
    printf("  get_Java_u2 = %d  [PASS]\n", val_u2);

    u1 data_u4[] = { 0xCA, 0xFE, 0xBA, 0xBE };
    u4 val_u4 = Bytes::get_Java_u4((address)data_u4);
    vm_assert(val_u4 == 0xCAFEBABE, "u4 byte swap failed");
    printf("  get_Java_u4 = 0x%08X  [PASS]\n", val_u4);
    printf("\n");
}

// ============================================================================
// Phase 2 测试：ClassFileParser
// ============================================================================

void test_classfile_stream() {
    printf("=== Test: ClassFileStream ===\n");
    u1 data[] = {
        0xCA, 0xFE, 0xBA, 0xBE,
        0x00, 0x00,
        0x00, 0x37,
        0x00, 0x05,
    };
    ClassFileStream stream(data, sizeof(data), "test_data");
    vm_assert(stream.get_u4() == 0xCAFEBABE, "magic");
    vm_assert(stream.get_u2() == 0, "minor");
    vm_assert(stream.get_u2() == 55, "major");
    vm_assert(stream.get_u2() == 5, "cp_count");
    vm_assert(stream.at_eos(), "eos");
    printf("  [PASS] ClassFileStream OK\n\n");
}

// ============================================================================
// Phase 3 测试 1：Mark Word
// ============================================================================

void test_mark_word() {
    printf("=== Test: Mark Word ===\n");

    // prototype() 应该是 unlocked，hash=0，age=0
    markOop proto = markOopDesc::prototype();
    printf("  prototype = 0x%016lx\n", (unsigned long)(uintptr_t)proto);

    vm_assert(proto->is_unlocked(), "prototype should be unlocked");
    vm_assert(proto->is_neutral(), "prototype should be neutral");
    vm_assert(!proto->is_locked(), "prototype should not be locked");
    vm_assert(!proto->is_marked(), "prototype should not be marked");
    vm_assert(!proto->has_bias_pattern(), "prototype should not be biased");
    vm_assert(proto->has_no_hash(), "prototype should have no hash");
    vm_assert(proto->age() == 0, "prototype age should be 0");

    // 测试 set_age
    markOop aged = proto->set_age(5);
    vm_assert(aged->age() == 5, "age should be 5");
    vm_assert(aged->is_unlocked(), "should still be unlocked");

    // 测试 incr_age
    markOop aged2 = aged->incr_age();
    vm_assert(aged2->age() == 6, "age should be 6");

    // 测试 copy_set_hash
    markOop hashed = proto->copy_set_hash(0x12345678);
    vm_assert(hashed->hash() == 0x12345678, "hash mismatch");
    vm_assert(hashed->is_unlocked(), "should still be unlocked");

    // 测试 set_marked (GC)
    markOop marked = proto->set_marked();
    vm_assert(marked->is_marked(), "should be marked");

    printf("  Mark Word bit layout:\n");
    printf("    lock_bits=%d, biased_lock_bits=%d, age_bits=%d, hash_bits=%d\n",
           markOopDesc::lock_bits, markOopDesc::biased_lock_bits,
           markOopDesc::age_bits, markOopDesc::hash_bits);
    printf("    age_shift=%d, hash_shift=%d\n",
           markOopDesc::age_shift, markOopDesc::hash_shift);
    printf("    lock_mask=0x%lx, age_mask=0x%lx\n",
           (unsigned long)markOopDesc::lock_mask,
           (unsigned long)markOopDesc::age_mask);
    printf("    hash_mask=0x%016lx\n", (unsigned long)markOopDesc::hash_mask);

    printf("  [PASS] Mark Word OK\n\n");
}

// ============================================================================
// Phase 3 测试 2：oopDesc 头部大小
// ============================================================================

void test_oop_header() {
    printf("=== Test: oopDesc Header ===\n");

    printf("  sizeof(oopDesc) = %lu bytes\n", (unsigned long)sizeof(oopDesc));
    printf("  oopDesc::header_size() = %d HeapWords\n", oopDesc::header_size());
    printf("  sizeof(instanceOopDesc) = %lu bytes\n", (unsigned long)sizeof(instanceOopDesc));
    printf("  instanceOopDesc::header_size() = %d HeapWords\n", instanceOopDesc::header_size());
    printf("  instanceOopDesc::base_offset_in_bytes() = %d\n", instanceOopDesc::base_offset_in_bytes());

    // LP64 非压缩指针下：
    vm_assert(sizeof(oopDesc) == 16, "oopDesc should be 16 bytes on LP64");
    vm_assert(oopDesc::header_size() == 2, "header should be 2 HeapWords");
    vm_assert(instanceOopDesc::base_offset_in_bytes() == 16, "base offset should be 16");

    // 偏移量
    printf("  mark_offset = %d\n", oopDesc::mark_offset_in_bytes());
    printf("  klass_offset = %d\n", oopDesc::klass_offset_in_bytes());
    vm_assert(oopDesc::mark_offset_in_bytes() == 0, "mark at offset 0");
    vm_assert(oopDesc::klass_offset_in_bytes() == 8, "klass at offset 8");

    printf("  [PASS] oopDesc Header OK\n\n");
}

// ============================================================================
// Phase 3 测试 3：AccessFlags
// ============================================================================

void test_access_flags() {
    printf("=== Test: AccessFlags ===\n");

    // 测试一个 public final class 的标志
    AccessFlags flags(JVM_ACC_PUBLIC | JVM_ACC_FINAL | JVM_ACC_SUPER);
    vm_assert(flags.is_public(), "should be public");
    vm_assert(flags.is_final(), "should be final");
    vm_assert(flags.is_super(), "should have ACC_SUPER");
    vm_assert(!flags.is_interface(), "should not be interface");
    vm_assert(!flags.is_abstract(), "should not be abstract");

    printf("  Public final class flags: ");
    flags.print_on(stdout);
    printf("\n");

    // 测试 Method 的 HotSpot 扩展标志
    AccessFlags mflags(JVM_ACC_PUBLIC | JVM_ACC_STATIC);
    mflags.set_has_linenumber_table();
    vm_assert(mflags.is_public(), "should be public");
    vm_assert(mflags.is_static(), "should be static");
    vm_assert(mflags.has_linenumber_table(), "should have linenumber table");

    printf("  Public static method flags: ");
    mflags.print_on(stdout);
    printf("\n");

    // 测试 get_flags() 只返回 .class 文件标志
    jint written = mflags.get_flags();
    vm_assert(written == (JVM_ACC_PUBLIC | JVM_ACC_STATIC),
              "get_flags() should mask HotSpot bits");

    printf("  [PASS] AccessFlags OK\n\n");
}

// ============================================================================
// Phase 3 测试 4：Metadata 和 Method
// ============================================================================

void test_metadata_method() {
    printf("=== Test: Metadata / Method ===\n");

    // 创建一个简单的 ConstantPool（1 个 slot）
    ConstantPool* cp = new ConstantPool(5);
    cp->utf8_at_put(1, (const u1*)"testMethod", 10);
    cp->utf8_at_put(2, (const u1*)"()V", 3);

    // 创建 ConstMethod
    ConstMethod* cm = new ConstMethod(cp, 3, 1, 1, 1, 2);
    u1 bytecodes[] = { 0xB1, 0x00, 0x00 }; // return
    cm->set_bytecodes(bytecodes, 3);

    // 创建 Method
    Method* m = new Method(cm, AccessFlags(JVM_ACC_PUBLIC));

    // 验证 Metadata 虚函数
    Metadata* meta = m;
    vm_assert(meta->is_method(), "should be method");
    vm_assert(!meta->is_klass(), "should not be klass");

    // 验证 Method 字段
    vm_assert(m->code_size() == 3, "code size should be 3");
    vm_assert(m->max_stack() == 1, "max_stack should be 1");
    vm_assert(m->max_locals() == 1, "max_locals should be 1");
    vm_assert(m->name_index() == 1, "name_index should be 1");
    vm_assert(m->signature_index() == 2, "signature_index should be 2");
    vm_assert(m->is_public(), "should be public");

    // 验证字节码
    vm_assert(cm->bytecode_at(0) == 0xB1, "first bytecode should be 0xB1 (return)");

    printf("  Method: ");
    m->print_on(stdout);
    printf("\n");

    // 清理
    delete m;  // 这会同时删除 cm
    delete cp;

    printf("  [PASS] Metadata/Method OK\n\n");
}

// ============================================================================
// Phase 3 测试 5：Klass 和 InstanceKlass
// ============================================================================

void test_klass() {
    printf("=== Test: Klass / InstanceKlass ===\n");

    InstanceKlass* ik = new InstanceKlass();

    // 验证 Metadata 虚函数
    Metadata* meta = ik;
    vm_assert(meta->is_klass(), "should be klass");
    vm_assert(!meta->is_method(), "should not be method");

    // 验证 KlassID
    vm_assert(ik->id() == InstanceKlassID, "should be InstanceKlassID");

    // 验证初始状态
    vm_assert(ik->init_state() == InstanceKlass::allocated, "initial state should be allocated");
    vm_assert(!ik->is_loaded(), "should not be loaded yet");

    // 设置基本信息
    ik->set_class_name("com/test/MyClass");
    ik->set_super_class_name("java/lang/Object");
    ik->set_access_flags(AccessFlags(JVM_ACC_PUBLIC | JVM_ACC_SUPER));
    ik->set_instance_size(32);

    vm_assert(ik->is_instance_klass(), "should be instance klass (positive layout_helper)");
    vm_assert(ik->instance_size() == 32, "instance size should be 32");
    vm_assert(strcmp(ik->name(), "com/test/MyClass") == 0, "name mismatch");

    printf("  ");
    ik->print_on(stdout);
    printf("\n");

    delete ik;
    printf("  [PASS] Klass/InstanceKlass OK\n\n");
}

// ============================================================================
// Phase 3 测试 6：完整流程 — 解析 .class → 创建 InstanceKlass
// ============================================================================

void test_full_pipeline(const char* path) {
    printf("=== Test: Full Pipeline (ClassFile → InstanceKlass) ===\n");
    printf("  File: %s\n\n", path);

    // 读取文件
    int length = 0;
    u1* buffer = read_class_file(path, &length);
    if (buffer == nullptr) {
        printf("  [SKIP] File not found.\n\n");
        return;
    }
    printf("  File size: %d bytes\n", length);

    // 创建流
    ClassFileStream stream(buffer, length, path);

    // 解析
    ClassFileParser parser(&stream);
    parser.parse();

    // 打印解析结果
    printf("\n  --- ClassFileParser Result ---\n");
    parser.print_summary(stdout);

    // 创建 InstanceKlass
    InstanceKlass* ik = parser.create_instance_klass();
    guarantee(ik != nullptr, "create_instance_klass() returned null");

    // 打印 InstanceKlass 信息
    printf("  --- InstanceKlass Result ---\n");
    ik->print_summary(stdout);

    // 验证基本属性
    printf("  Verification:\n");
    printf("    is_klass() = %s\n", ik->is_klass() ? "true" : "false");
    printf("    is_instance_klass() = %s\n", ik->is_instance_klass() ? "true" : "false");
    printf("    is_loaded() = %s\n", ik->is_loaded() ? "true" : "false");
    printf("    instance_size() = %d bytes\n", ik->instance_size());
    printf("    layout_helper() = %d\n", ik->layout_helper());

    vm_assert(ik->is_klass(), "should be klass");
    vm_assert(ik->is_instance_klass(), "should be instance klass");
    vm_assert(ik->is_loaded(), "should be loaded");
    vm_assert(ik->methods_count() > 0, "should have methods");
    vm_assert(ik->instance_size() > 0, "instance size should be positive");

    // 测试方法查找
    Method* main_method = ik->find_method("main", "([Ljava/lang/String;)V");
    if (main_method != nullptr) {
        printf("    Found main(): code_size=%d, max_stack=%d, max_locals=%d\n",
               main_method->code_size(), main_method->max_stack(), main_method->max_locals());
        vm_assert(main_method->is_public(), "main should be public");
        vm_assert(main_method->is_static(), "main should be static");
    } else {
        printf("    main() not found (OK if not a main class)\n");
    }

    // 验证 oopDesc header 与 instance size 的关系
    printf("\n  Object Layout:\n");
    printf("    oopDesc header:   %d bytes\n", (int)sizeof(oopDesc));
    printf("    instance total:   %d bytes\n", ik->instance_size());
    printf("    field data:       %d bytes\n",
           ik->instance_size() - (int)sizeof(oopDesc));

    // 清理
    // InstanceKlass 析构时会释放 ConstantPool、Methods、FieldInfos
    delete ik;

    FREE_C_HEAP_ARRAY(u1, buffer);

    printf("\n  [PASS] Full Pipeline OK\n\n");
}

// ============================================================================
// Phase 4 测试 1：字节码枚举
// ============================================================================

void test_bytecodes() {
    printf("=== Test: Bytecodes ===\n");

    // 验证基本字节码值
    vm_assert(Bytecodes::_nop == 0x00, "nop should be 0x00");
    vm_assert(Bytecodes::_iconst_0 == 0x03, "iconst_0 should be 0x03");
    vm_assert(Bytecodes::_iadd == 0x60, "iadd should be 0x60");
    vm_assert(Bytecodes::_iand == 0x7E, "iand should be 0x7E");
    vm_assert(Bytecodes::_iinc == 0x84, "iinc should be 0x84");
    vm_assert(Bytecodes::_i2l == 0x85, "i2l should be 0x85");
    vm_assert(Bytecodes::_ireturn == 0xAC, "ireturn should be 0xAC");
    vm_assert(Bytecodes::_return == 0xB1, "return should be 0xB1");
    vm_assert(Bytecodes::_invokevirtual == 0xB6, "invokevirtual should be 0xB6");
    vm_assert(Bytecodes::_new == 0xBB, "new should be 0xBB");

    // 验证长度
    vm_assert(Bytecodes::length_for(Bytecodes::_nop) == 1, "nop length=1");
    vm_assert(Bytecodes::length_for(Bytecodes::_bipush) == 2, "bipush length=2");
    vm_assert(Bytecodes::length_for(Bytecodes::_sipush) == 3, "sipush length=3");
    vm_assert(Bytecodes::length_for(Bytecodes::_goto) == 3, "goto length=3");
    vm_assert(Bytecodes::length_for(Bytecodes::_invokestatic) == 3, "invokestatic length=3");

    // 验证名称
    vm_assert(strcmp(Bytecodes::name(Bytecodes::_iadd), "iadd") == 0, "name of iadd");
    vm_assert(strcmp(Bytecodes::name(Bytecodes::_return), "return") == 0, "name of return");

    printf("  Bytecode values and lengths verified\n");
    printf("  [PASS] Bytecodes OK\n\n");
}

// ============================================================================
// Phase 4 测试 2：JavaThread
// ============================================================================

void test_java_thread() {
    printf("=== Test: JavaThread ===\n");

    JavaThread thread("test-thread");

    // 初始状态
    vm_assert(thread.thread_state() == _thread_new, "initial state should be _thread_new");
    vm_assert(!thread.has_pending_exception(), "no exception initially");
    vm_assert(strcmp(thread.name(), "test-thread") == 0, "thread name");

    // 状态切换
    thread.set_thread_state(_thread_in_Java);
    vm_assert(thread.is_in_java(), "should be in Java");

    thread.set_thread_state(_thread_in_vm);
    vm_assert(thread.is_in_vm(), "should be in VM");

    // 异常
    thread.set_pending_exception((oopDesc*)(intptr_t)0xDEAD, "test exception");
    vm_assert(thread.has_pending_exception(), "should have exception");
    vm_assert(strcmp(thread.exception_message(), "test exception") == 0, "exception message");

    thread.clear_pending_exception();
    vm_assert(!thread.has_pending_exception(), "exception should be cleared");

    printf("  JavaThread state management verified\n");
    printf("  [PASS] JavaThread OK\n\n");
}

// ============================================================================
// Phase 4 测试 3：InterpreterFrame 基本操作
// ============================================================================

void test_interpreter_frame() {
    printf("=== Test: InterpreterFrame ===\n");

    // 创建一个简单方法（3字节码：iconst_3, iconst_4, iadd）
    ConstantPool* cp = new ConstantPool(5);
    cp->utf8_at_put(1, (const u1*)"test", 4);
    cp->utf8_at_put(2, (const u1*)"(II)I", 5);

    ConstMethod* cm = new ConstMethod(cp, 3, 4, 4, 1, 2);
    u1 bytecodes[] = { Bytecodes::_iconst_3, Bytecodes::_iconst_4, Bytecodes::_iadd };
    cm->set_bytecodes(bytecodes, 3);

    Method* method = new Method(cm, AccessFlags(JVM_ACC_PUBLIC | JVM_ACC_STATIC));

    // 创建帧
    InterpreterFrame frame(method, cp, nullptr);

    // 验证初始状态
    vm_assert(frame.bci() == 0, "initial bci=0");
    vm_assert(frame.sp() == 0, "initial sp=0");
    vm_assert(frame.stack_is_empty(), "stack should be empty");
    vm_assert(frame.max_stack() == 4, "max_stack=4");
    vm_assert(frame.max_locals() == 4, "max_locals=4");

    // 测试局部变量
    frame.set_local_int(0, 42);
    frame.set_local_int(1, -7);
    vm_assert(frame.local_int(0) == 42, "local[0]=42");
    vm_assert(frame.local_int(1) == -7, "local[1]=-7");

    // 测试操作数栈
    frame.push_int(10);
    frame.push_int(20);
    vm_assert(frame.sp() == 2, "sp=2 after 2 pushes");
    vm_assert(frame.peek_int(0) == 20, "top=20");
    vm_assert(frame.peek_int(1) == 10, "below top=10");

    jint v2 = frame.pop_int();
    jint v1 = frame.pop_int();
    vm_assert(v2 == 20, "pop 20");
    vm_assert(v1 == 10, "pop 10");
    vm_assert(frame.stack_is_empty(), "stack empty after 2 pops");

    // 测试 BCP
    vm_assert(frame.current_bytecode() == Bytecodes::_iconst_3, "first bytecode is iconst_3");
    frame.advance_bcp(1);
    vm_assert(frame.bci() == 1, "bci=1 after advance");
    vm_assert(frame.current_bytecode() == Bytecodes::_iconst_4, "second bytecode is iconst_4");

    printf("  InterpreterFrame locals/stack/BCP verified\n");

    // 清理
    delete method;  // 会同时删除 cm
    delete cp;

    printf("  [PASS] InterpreterFrame OK\n\n");
}

// ============================================================================
// Phase 4 测试 4：手动构建方法并用解释器执行
// 测试简单的 iadd 操作：iconst_3 + iconst_4 = 7
// ============================================================================

void test_interpreter_simple_add() {
    printf("=== Test: Interpreter Simple Add (3+4) ===\n");

    // 创建常量池
    ConstantPool* cp = new ConstantPool(5);
    cp->utf8_at_put(1, (const u1*)"simpleAdd", 9);
    cp->utf8_at_put(2, (const u1*)"()I", 3);

    // 创建方法字节码：iconst_3, iconst_4, iadd, ireturn
    u1 code[] = {
        Bytecodes::_iconst_3,   // 0: iconst_3
        Bytecodes::_iconst_4,   // 1: iconst_4
        (u1)Bytecodes::_iadd,   // 2: iadd
        (u1)Bytecodes::_ireturn // 3: ireturn
    };
    ConstMethod* cm = new ConstMethod(cp, 4, 2, 1, 1, 2);
    cm->set_bytecodes(code, 4);

    Method* method = new Method(cm, AccessFlags(JVM_ACC_PUBLIC | JVM_ACC_STATIC));

    // 创建线程
    JavaThread thread("test");

    // 创建 InstanceKlass（解释器需要它来访问常量池）
    InstanceKlass* klass = new InstanceKlass();
    klass->set_class_name("TestClass");
    klass->set_constants(cp);

    // 执行
    JavaValue result(T_INT);
    BytecodeInterpreter::_trace_bytecodes = true;
    BytecodeInterpreter::execute(method, klass, &thread, &result);
    BytecodeInterpreter::_trace_bytecodes = false;

    // 验证结果
    printf("  Result: %d (expected 7)\n", result.get_jint());
    vm_assert(result.get_jint() == 7, "3 + 4 should be 7");
    vm_assert(!thread.has_pending_exception(), "no exception");

    // 清理 — 这里需要小心所有权
    // klass 拥有 cp 的所有权，method 拥有 cm 的所有权
    // 但 klass->set_constants(cp) 设置了关联，析构时 klass 会 delete cp
    // method 不被 klass 管理（没有 set_methods），需要手动删除
    // 先删 method（它会 delete cm），再删 klass（它会 delete cp）
    delete method;
    // 防止 klass 析构时再次删除 cp，因为 method 的 cm 中已有 cp 引用
    // 但 klass 拥有 cp 所有权——实际上 ConstMethod 不删除 cp，只有 InstanceKlass 删除
    // 所以直接删除 klass 即可
    delete klass;  // 会 delete cp

    printf("  [PASS] Interpreter Simple Add OK\n\n");
}

// ============================================================================
// Phase 4 测试 5：带参数的方法调用
// 模拟 add(int a, int b) { return a + b; } 调用 add(10, 25) = 35
// ============================================================================

void test_interpreter_with_args() {
    printf("=== Test: Interpreter With Args (add(10, 25)=35) ===\n");

    // 创建常量池
    ConstantPool* cp = new ConstantPool(5);
    cp->utf8_at_put(1, (const u1*)"add", 3);
    cp->utf8_at_put(2, (const u1*)"(II)I", 5);

    // 字节码：iload_0, iload_1, iadd, ireturn
    u1 code[] = {
        Bytecodes::_iload_0,    // 0: iload_0  (参数 a)
        Bytecodes::_iload_1,    // 1: iload_1  (参数 b)
        (u1)Bytecodes::_iadd,   // 2: iadd
        (u1)Bytecodes::_ireturn // 3: ireturn
    };
    ConstMethod* cm = new ConstMethod(cp, 4, 2, 2, 1, 2);
    cm->set_bytecodes(code, 4);

    Method* method = new Method(cm, AccessFlags(JVM_ACC_PUBLIC | JVM_ACC_STATIC));

    JavaThread thread("test");
    InstanceKlass* klass = new InstanceKlass();
    klass->set_class_name("TestClass");
    klass->set_constants(cp);

    // 准备参数
    intptr_t args[2] = { 10, 25 };

    JavaValue result(T_INT);
    BytecodeInterpreter::execute(method, klass, &thread, &result, args, 2);

    printf("  Result: %d (expected 35)\n", result.get_jint());
    vm_assert(result.get_jint() == 35, "10 + 25 should be 35");
    vm_assert(!thread.has_pending_exception(), "no exception");

    delete method;
    delete klass;

    printf("  [PASS] Interpreter With Args OK\n\n");
}

// ============================================================================
// Phase 4 测试 6：条件分支 (计算绝对值)
// if (x < 0) x = -x; return x;
// ============================================================================

void test_interpreter_branch() {
    printf("=== Test: Interpreter Branch (abs(-42)=42) ===\n");

    ConstantPool* cp = new ConstantPool(5);
    cp->utf8_at_put(1, (const u1*)"abs", 3);
    cp->utf8_at_put(2, (const u1*)"(I)I", 4);

    // 字节码：
    // 0: iload_0       (加载参数)
    // 1: ifge 7        (如果 >= 0，跳到 7)  → offset = +6 = {00, 06}
    // 4: iload_0       (加载参数)
    // 5: ineg          (取反)
    // 6: ireturn       (返回)
    // 7: iload_0       (加载参数)
    // 8: ireturn       (返回)
    u1 code[] = {
        Bytecodes::_iload_0,                // 0: iload_0
        (u1)Bytecodes::_ifge, 0x00, 0x06,  // 1: ifge +6 → bci 7
        Bytecodes::_iload_0,                // 4: iload_0
        (u1)Bytecodes::_ineg,               // 5: ineg
        (u1)Bytecodes::_ireturn,            // 6: ireturn
        Bytecodes::_iload_0,                // 7: iload_0
        (u1)Bytecodes::_ireturn,            // 8: ireturn
    };
    ConstMethod* cm = new ConstMethod(cp, 9, 2, 1, 1, 2);
    cm->set_bytecodes(code, 9);

    Method* method = new Method(cm, AccessFlags(JVM_ACC_PUBLIC | JVM_ACC_STATIC));

    JavaThread thread("test");
    InstanceKlass* klass = new InstanceKlass();
    klass->set_class_name("TestClass");
    klass->set_constants(cp);

    // 测试 abs(-42) = 42
    {
        intptr_t args[1] = { -42 };
        JavaValue result(T_INT);
        BytecodeInterpreter::execute(method, klass, &thread, &result, args, 1);
        printf("  abs(-42) = %d (expected 42)\n", result.get_jint());
        vm_assert(result.get_jint() == 42, "abs(-42) should be 42");
    }

    // 测试 abs(99) = 99
    {
        intptr_t args[1] = { 99 };
        JavaValue result(T_INT);
        BytecodeInterpreter::execute(method, klass, &thread, &result, args, 1);
        printf("  abs(99) = %d (expected 99)\n", result.get_jint());
        vm_assert(result.get_jint() == 99, "abs(99) should be 99");
    }

    delete method;
    delete klass;

    printf("  [PASS] Interpreter Branch OK\n\n");
}

// ============================================================================
// Phase 4 测试 7：完整流程 — 解析 .class → 执行 main()
// HelloWorld.main() 最终应该打印 "7"
// ============================================================================

void test_full_execution(const char* path) {
    printf("=== Test: Full Execution (ClassFile → Interpret) ===\n");
    printf("  File: %s\n\n", path);

    // 读取文件
    int length = 0;
    u1* buffer = read_class_file(path, &length);
    if (buffer == nullptr) {
        printf("  [SKIP] File not found.\n\n");
        return;
    }

    // 解析
    ClassFileStream stream(buffer, length, path);
    ClassFileParser parser(&stream);
    parser.parse();
    InstanceKlass* ik = parser.create_instance_klass();
    guarantee(ik != nullptr, "create_instance_klass() returned null");

    printf("  InstanceKlass created: %s\n", ik->class_name());
    printf("  Methods: %d\n", ik->methods_count());

    // 查找 main 方法
    Method* main_method = ik->find_method("main", "([Ljava/lang/String;)V");
    if (main_method == nullptr) {
        printf("  [SKIP] main() not found.\n\n");
        delete ik;
        FREE_C_HEAP_ARRAY(u1, buffer);
        return;
    }

    printf("  Found main(): code_size=%d, max_stack=%d, max_locals=%d\n",
           main_method->code_size(), main_method->max_stack(), main_method->max_locals());

    // 打印 main 的字节码
    printf("  main() bytecodes:\n    ");
    for (int i = 0; i < main_method->code_size(); i++) {
        printf("%02X ", main_method->code_base()[i]);
        if ((i + 1) % 16 == 0) printf("\n    ");
    }
    printf("\n\n");

    // 创建线程
    JavaThread thread("main");

    // 开启 trace 模式
    BytecodeInterpreter::_trace_bytecodes = true;

    // 执行 main()
    // main(String[]) 有一个参数 args，我们传 null（0）
    intptr_t args[1] = { 0 };  // args = null
    JavaValue result(T_VOID);

    printf("  === Executing main() ===\n");
    BytecodeInterpreter::execute(main_method, ik, &thread, &result, args, 1);

    BytecodeInterpreter::_trace_bytecodes = false;

    if (thread.has_pending_exception()) {
        printf("  Exception: %s\n", thread.exception_message());
    } else {
        printf("  === main() completed successfully ===\n");
    }

    // 清理
    delete ik;
    FREE_C_HEAP_ARRAY(u1, buffer);

    printf("  [PASS] Full Execution OK\n\n");
}

// ============================================================================
// Phase 5 测试 1：JavaHeap 基本分配
// ============================================================================

void test_java_heap_basic() {
    printf("=== Test: JavaHeap Basic Allocation ===\n");

    // 初始化堆（1MB）
    JavaHeap::initialize(1 * 1024 * 1024);
    JavaHeap* heap = JavaHeap::heap();

    vm_assert(heap != nullptr, "heap should be initialized");
    vm_assert(heap->capacity() == 1 * 1024 * 1024, "capacity should be 1MB");
    vm_assert(heap->used() == 0, "used should be 0");
    vm_assert(heap->free() == 1 * 1024 * 1024, "free should be 1MB");

    // 分配一些内存
    HeapWord* p1 = heap->allocate(2);  // 16 bytes
    vm_assert(p1 != nullptr, "first allocation should succeed");
    vm_assert(heap->used() == 16, "used should be 16");
    vm_assert(heap->is_in(p1), "p1 should be in heap");

    HeapWord* p2 = heap->allocate(4);  // 32 bytes
    vm_assert(p2 != nullptr, "second allocation should succeed");
    vm_assert(heap->used() == 48, "used should be 48");
    vm_assert(p2 > p1, "p2 should be after p1 (bump pointer)");

    // 打印堆状态
    heap->print_on(stdout);

    // 清理
    JavaHeap::destroy();

    printf("  [PASS] JavaHeap Basic Allocation OK\n\n");
}

// ============================================================================
// Phase 5 测试 2：对象分配（手动创建 InstanceKlass + 分配实例）
// ============================================================================

void test_object_allocation() {
    printf("=== Test: Object Allocation ===\n");

    // 初始化堆
    JavaHeap::initialize(1 * 1024 * 1024);

    // 创建一个简单的类：
    //   class Point { int x; int y; }
    //   instance_size = 16 (header) + 4 (x) + 4 (y) = 24 → aligned to 24 bytes
    ConstantPool* cp = new ConstantPool(10);
    cp->utf8_at_put(1, (const u1*)"Point", 5);
    cp->utf8_at_put(2, (const u1*)"x", 1);
    cp->utf8_at_put(3, (const u1*)"I", 1);
    cp->utf8_at_put(4, (const u1*)"y", 1);
    cp->utf8_at_put(5, (const u1*)"I", 1);

    // 字段信息
    FieldInfoEntry* fields = NEW_C_HEAP_ARRAY(FieldInfoEntry, 2, mtClass);
    fields[0] = { 0, 2, 3, FieldInfoEntry::invalid_offset, 0 }; // x: int
    fields[1] = { 0, 4, 5, FieldInfoEntry::invalid_offset, 0 }; // y: int

    InstanceKlass* ik = new InstanceKlass();
    ik->set_class_name("test/Point");
    ik->set_constants(cp);
    ik->set_fields(fields, 2);

    // 手动计算并设置实例大小
    int instance_size = instanceOopDesc::base_offset_in_bytes() + 4 + 4; // 16 + 4 + 4 = 24
    instance_size = (int)align_up((uintx)instance_size, (uintx)HeapWordSize);
    ik->set_instance_size(instance_size);

    // 设置字段偏移
    fields[0].offset = (u2)instanceOopDesc::base_offset_in_bytes();      // x at 16
    fields[1].offset = (u2)(instanceOopDesc::base_offset_in_bytes() + 4); // y at 20

    printf("  Class: %s, instance_size=%d\n", ik->class_name(), ik->instance_size());
    printf("  Field x: offset=%d\n", fields[0].offset);
    printf("  Field y: offset=%d\n", fields[1].offset);

    // 分配实例
    oopDesc* obj = ik->allocate_instance();
    vm_assert(obj != nullptr, "object allocation should succeed");

    printf("  Allocated object at %p\n", (void*)obj);

    // 验证对象头
    vm_assert(obj->klass() == ik, "klass pointer should point to InstanceKlass");
    vm_assert(obj->mark()->is_unlocked(), "mark should be unlocked");
    vm_assert(obj->mark()->age() == 0, "age should be 0");

    // 测试字段读写
    int x_offset = fields[0].offset;
    int y_offset = fields[1].offset;

    // 初始值应该是 0（内存已清零）
    vm_assert(obj->int_field(x_offset) == 0, "x should initially be 0");
    vm_assert(obj->int_field(y_offset) == 0, "y should initially be 0");

    // 写入字段
    obj->int_field_put(x_offset, 42);
    obj->int_field_put(y_offset, 99);

    // 读回验证
    vm_assert(obj->int_field(x_offset) == 42, "x should be 42");
    vm_assert(obj->int_field(y_offset) == 99, "y should be 99");

    printf("  x=%d, y=%d\n", obj->int_field(x_offset), obj->int_field(y_offset));

    // 分配第二个对象
    oopDesc* obj2 = ik->allocate_instance();
    vm_assert(obj2 != nullptr, "second object should succeed");
    vm_assert(obj2 > obj, "second object should be at higher address");
    vm_assert(obj2->int_field(x_offset) == 0, "obj2.x should be 0");

    printf("  Second object at %p (gap=%ld bytes)\n",
           (void*)obj2, (long)((char*)obj2 - (char*)obj));

    // 堆状态
    JavaHeap::heap()->print_on(stdout);

    // 清理（堆上的对象不需要单独释放，整个堆一起释放）
    delete ik;  // 会删除 cp 和 fields
    JavaHeap::destroy();

    printf("  [PASS] Object Allocation OK\n\n");
}

// ============================================================================
// Phase 5 测试 3：解释器中的对象创建和字段访问
// 手动构建字节码序列模拟：
//
//   class Counter {
//       int count;
//       static int add(int a, int b) { return a + b; }
//   }
//
// 测试的字节码（手动构建）：
//   new Counter          // 创建对象
//   dup                  // 复制引用
//   invokespecial <init> // 调用构造函数（我们跳过）
//   astore_1             // 存储到 local[1]
//   aload_1              // 加载对象引用
//   bipush 42            // 压入 42
//   putfield count       // 设置 count=42
//   aload_1              // 加载对象引用
//   getfield count       // 获取 count
//   ireturn              // 返回 count 的值
// ============================================================================

void test_interpreter_object_creation() {
    printf("=== Test: Interpreter Object Creation & Field Access ===\n");

    // 初始化堆
    JavaHeap::initialize(1 * 1024 * 1024);

    // 创建常量池
    // 需要：
    //   #1 = Class "test/Counter"
    //   #2 = UTF8 "test/Counter"
    //   #3 = UTF8 "count"
    //   #4 = UTF8 "I"
    //   #5 = UTF8 "test"
    //   #6 = UTF8 "(I)I"
    //   #7 = NameAndType #3:#4  (count:I)
    //   #8 = Fieldref #1.#7     (test/Counter.count:I)
    //   #9 = UTF8 "<init>"
    //   #10 = UTF8 "()V"
    //   #11 = NameAndType #9:#10  (<init>:()V)
    //   #12 = Methodref #1.#11   (test/Counter.<init>:()V)
    ConstantPool* cp = new ConstantPool(20);

    cp->utf8_at_put(2, (const u1*)"test/Counter", 12);
    cp->klass_index_at_put(1, 2);     // #1 = Class → #2

    cp->utf8_at_put(3, (const u1*)"count", 5);
    cp->utf8_at_put(4, (const u1*)"I", 1);

    cp->utf8_at_put(5, (const u1*)"test", 4);
    cp->utf8_at_put(6, (const u1*)"(I)I", 4);

    cp->name_and_type_at_put(7, 3, 4);    // #7 = NameAndType count:I
    cp->field_at_put(8, 1, 7);             // #8 = Fieldref Counter.count:I

    cp->utf8_at_put(9, (const u1*)"<init>", 6);
    cp->utf8_at_put(10, (const u1*)"()V", 3);
    cp->name_and_type_at_put(11, 9, 10);   // #11 = NameAndType <init>:()V
    cp->method_at_put(12, 1, 11);           // #12 = Methodref Counter.<init>:()V

    // 字段信息：一个非静态字段 count (int)
    FieldInfoEntry* fields = NEW_C_HEAP_ARRAY(FieldInfoEntry, 1, mtClass);
    fields[0] = { 0, 3, 4, (u2)instanceOopDesc::base_offset_in_bytes(), 0 }; // count at offset 16

    // 创建 InstanceKlass
    InstanceKlass* ik = new InstanceKlass();
    ik->set_class_name("test/Counter");
    ik->set_constants(cp);
    ik->set_fields(fields, 1);
    ik->set_has_nonstatic_fields();

    // instance_size = 16 (header) + 4 (count) = 20 → aligned to 24
    int inst_size = (int)align_up((uintx)(instanceOopDesc::base_offset_in_bytes() + 4), (uintx)HeapWordSize);
    ik->set_instance_size(inst_size);
    ik->set_init_state(InstanceKlass::fully_initialized);

    printf("  Class: %s, instance_size=%d\n", ik->class_name(), ik->instance_size());

    // 构建测试方法字节码：
    //   0: new #1              (BB 00 01)
    //   3: dup                 (59)
    //   4: invokespecial #12   (B7 00 0C) — <init>
    //   7: astore_1            (4C)
    //   8: aload_1             (2B)
    //   9: bipush 42           (10 2A)
    //  11: putfield #8         (B5 00 08)
    //  14: aload_1             (2B)
    //  15: getfield #8         (B4 00 08)
    //  18: ireturn             (AC)
    u1 code[] = {
        0xBB, 0x00, 0x01,   // 0: new #1 (test/Counter)
        0x59,                // 3: dup
        0xB7, 0x00, 0x0C,   // 4: invokespecial #12 (<init>)
        0x4C,                // 7: astore_1
        0x2B,                // 8: aload_1
        0x10, 0x2A,          // 9: bipush 42
        0xB5, 0x00, 0x08,   // 11: putfield #8 (count)
        0x2B,                // 14: aload_1
        0xB4, 0x00, 0x08,   // 15: getfield #8 (count)
        0xAC,                // 18: ireturn
    };

    cp->utf8_at_put(13, (const u1*)"testMethod", 10);
    cp->utf8_at_put(14, (const u1*)"()I", 3);

    ConstMethod* cm = new ConstMethod(cp, 19, 4, 2, 13, 14);
    cm->set_bytecodes(code, 19);

    Method* method = new Method(cm, AccessFlags(JVM_ACC_PUBLIC | JVM_ACC_STATIC));

    // 设置方法到 klass（这样 invokespecial <init> 能找到目标方法，
    // 不过我们的 <init> 是外部类方法，会被跳过）
    // 不需要设置 methods，因为 invoke_method 对外部类 <init> 已处理

    // 创建线程并执行
    JavaThread thread("test");
    JavaValue result(T_INT);

    BytecodeInterpreter::_trace_bytecodes = true;
    BytecodeInterpreter::execute(method, ik, &thread, &result);
    BytecodeInterpreter::_trace_bytecodes = false;

    // 验证结果
    printf("  Result: %d (expected 42)\n", result.get_jint());
    vm_assert(result.get_jint() == 42, "getfield should return 42 after putfield 42");
    vm_assert(!thread.has_pending_exception(), "no exception");

    // 堆状态
    JavaHeap::heap()->print_on(stdout);

    // 清理
    delete method;
    delete ik;
    JavaHeap::destroy();

    printf("  [PASS] Interpreter Object Creation & Field Access OK\n\n");
}

// ============================================================================
// Phase 5 测试 4：静态字段读写
// 手动构建字节码模拟：
//   static int counter;
//   putstatic counter = 100
//   getstatic counter
//   ireturn
// ============================================================================

void test_interpreter_static_fields() {
    printf("=== Test: Interpreter Static Fields ===\n");

    JavaHeap::initialize(1 * 1024 * 1024);

    // 常量池
    ConstantPool* cp = new ConstantPool(15);

    cp->utf8_at_put(2, (const u1*)"test/StaticTest", 15);
    cp->klass_index_at_put(1, 2);

    cp->utf8_at_put(3, (const u1*)"counter", 7);
    cp->utf8_at_put(4, (const u1*)"I", 1);
    cp->name_and_type_at_put(5, 3, 4);   // #5 = NameAndType counter:I
    cp->field_at_put(6, 1, 5);            // #6 = Fieldref StaticTest.counter:I

    // 字段：一个静态字段 counter (int)
    FieldInfoEntry* fields = NEW_C_HEAP_ARRAY(FieldInfoEntry, 1, mtClass);
    fields[0] = { JVM_ACC_STATIC, 3, 4, FieldInfoEntry::invalid_offset, 0 };

    // 使用 create_from_parser 来自动分配静态字段存储
    InstanceKlass* ik = InstanceKlass::create_from_parser(
        "test/StaticTest", "java/lang/Object",
        JVM_ACC_PUBLIC | JVM_ACC_SUPER,
        55, 0,
        cp,
        fields, 1,
        nullptr, 0
    );

    printf("  Class: %s\n", ik->class_name());
    printf("  Static field 'counter' index: %d\n", ik->static_field_index("counter"));

    // 验证静态字段初始值为 0
    int idx = ik->static_field_index("counter");
    vm_assert(idx == 0, "counter should be at static index 0");
    vm_assert(ik->static_field_value(idx) == 0, "initial value should be 0");

    // 构建字节码：
    //   0: bipush 100    (10 64)
    //   2: putstatic #6  (B3 00 06)
    //   5: getstatic #6  (B2 00 06)
    //   8: ireturn       (AC)
    u1 code[] = {
        0x10, 0x64,          // 0: bipush 100
        0xB3, 0x00, 0x06,    // 2: putstatic #6 (counter)
        0xB2, 0x00, 0x06,    // 5: getstatic #6 (counter)
        0xAC,                 // 8: ireturn
    };

    cp->utf8_at_put(9, (const u1*)"testStatic", 10);
    cp->utf8_at_put(10, (const u1*)"()I", 3);

    ConstMethod* cm = new ConstMethod(cp, 9, 2, 1, 9, 10);
    cm->set_bytecodes(code, 9);
    Method* method = new Method(cm, AccessFlags(JVM_ACC_PUBLIC | JVM_ACC_STATIC));

    JavaThread thread("test");
    JavaValue result(T_INT);

    BytecodeInterpreter::_trace_bytecodes = true;
    BytecodeInterpreter::execute(method, ik, &thread, &result);
    BytecodeInterpreter::_trace_bytecodes = false;

    printf("  Result: %d (expected 100)\n", result.get_jint());
    vm_assert(result.get_jint() == 100, "getstatic should return 100");
    vm_assert(!thread.has_pending_exception(), "no exception");

    // 验证静态字段确实被修改了
    vm_assert(ik->static_field_value(idx) == 100, "static field should be 100");

    delete method;
    delete ik;
    JavaHeap::destroy();

    printf("  [PASS] Interpreter Static Fields OK\n\n");
}

// ============================================================================
// Phase 6 测试 1：<init> 构造函数执行
//
// 模拟：
//   class Counter {
//       int count;
//       Counter() { this.count = 10; }
//   }
//
// 测试字节码序列：
//   new Counter          → 分配对象
//   dup                  → 复制引用
//   invokespecial <init> → 执行构造函数（this.count = 10）
//   astore_1             → 保存到 local[1]
//   aload_1              → 加载引用
//   getfield count       → 读取 count
//   ireturn              → 返回 count 的值（应该是 10）
//
// <init>()V 的字节码：
//   aload_0              → 加载 this
//   bipush 10            → 压入 10
//   putfield count       → this.count = 10
//   return               → void 返回
// ============================================================================

void test_init_constructor() {
    printf("=== Test: Phase 6 - <init> Constructor Execution ===\n");

    JavaHeap::initialize(1 * 1024 * 1024);

    // 常量池布局：
    //   #1  = Class      → #2 ("test/Counter")
    //   #2  = UTF8       "test/Counter"
    //   #3  = UTF8       "count"
    //   #4  = UTF8       "I"
    //   #5  = NameAndType #3:#4 (count:I)
    //   #6  = Fieldref   #1.#5 (test/Counter.count:I)
    //   #7  = UTF8       "<init>"
    //   #8  = UTF8       "()V"
    //   #9  = NameAndType #7:#8 (<init>:()V)
    //   #10 = Methodref  #1.#9 (test/Counter.<init>:()V)
    //   #11 = UTF8       "testMethod"
    //   #12 = UTF8       "()I"
    ConstantPool* cp = new ConstantPool(20);

    cp->utf8_at_put(2, (const u1*)"test/Counter", 12);
    cp->klass_index_at_put(1, 2);

    cp->utf8_at_put(3, (const u1*)"count", 5);
    cp->utf8_at_put(4, (const u1*)"I", 1);
    cp->name_and_type_at_put(5, 3, 4);     // #5 = NameAndType count:I
    cp->field_at_put(6, 1, 5);              // #6 = Fieldref Counter.count:I

    cp->utf8_at_put(7, (const u1*)"<init>", 6);
    cp->utf8_at_put(8, (const u1*)"()V", 3);
    cp->name_and_type_at_put(9, 7, 8);     // #9 = NameAndType <init>:()V
    cp->method_at_put(10, 1, 9);            // #10 = Methodref Counter.<init>:()V

    cp->utf8_at_put(11, (const u1*)"testMethod", 10);
    cp->utf8_at_put(12, (const u1*)"()I", 3);

    // ---- 构建 <init>()V 方法 ----
    // 字节码：
    //   0: aload_0       (2A)         → 加载 this
    //   1: bipush 10     (10 0A)      → 压入 10
    //   3: putfield #6   (B5 00 06)   → this.count = 10
    //   6: return        (B1)         → void 返回
    u1 init_code[] = {
        0x2A,                // 0: aload_0
        0x10, 0x0A,          // 1: bipush 10
        0xB5, 0x00, 0x06,    // 3: putfield #6 (count)
        0xB1,                // 6: return
    };
    ConstMethod* init_cm = new ConstMethod(cp, 7, 2, 1, 7, 8);
    // max_stack=2 (this + value), max_locals=1 (this)
    init_cm->set_bytecodes(init_code, 7);
    Method* init_method = new Method(init_cm, AccessFlags(JVM_ACC_PUBLIC));

    // ---- 构建 testMethod()I （调用者方法） ----
    // 字节码：
    //   0: new #1              (BB 00 01)
    //   3: dup                 (59)
    //   4: invokespecial #10   (B7 00 0A)  → <init>
    //   7: astore_1            (4C)
    //   8: aload_1             (2B)
    //   9: getfield #6         (B4 00 06)  → count
    //  12: ireturn             (AC)
    u1 test_code[] = {
        0xBB, 0x00, 0x01,   // 0: new #1 (test/Counter)
        0x59,                // 3: dup
        0xB7, 0x00, 0x0A,   // 4: invokespecial #10 (<init>)
        0x4C,                // 7: astore_1
        0x2B,                // 8: aload_1
        0xB4, 0x00, 0x06,   // 9: getfield #6 (count)
        0xAC,                // 12: ireturn
    };
    ConstMethod* test_cm = new ConstMethod(cp, 13, 4, 2, 11, 12);
    // max_stack=4, max_locals=2 (local[0]=unused, local[1]=obj)
    test_cm->set_bytecodes(test_code, 13);
    Method* test_method = new Method(test_cm, AccessFlags(JVM_ACC_PUBLIC | JVM_ACC_STATIC));

    // ---- 构建字段信息 ----
    FieldInfoEntry* fields = NEW_C_HEAP_ARRAY(FieldInfoEntry, 1, mtClass);
    fields[0] = { 0, 3, 4, (u2)instanceOopDesc::base_offset_in_bytes(), 0 }; // count at offset 16

    // ---- 构建方法数组（包含 <init>） ----
    Method** methods = NEW_C_HEAP_ARRAY(Method*, 1, mtClass);
    methods[0] = init_method;

    // ---- 创建 InstanceKlass ----
    InstanceKlass* ik = new InstanceKlass();
    ik->set_class_name("test/Counter");
    ik->set_constants(cp);
    ik->set_fields(fields, 1);
    ik->set_methods(methods, 1);
    ik->set_has_nonstatic_fields();

    int inst_size = (int)align_up((uintx)(instanceOopDesc::base_offset_in_bytes() + 4), (uintx)HeapWordSize);
    ik->set_instance_size(inst_size);
    ik->set_init_state(InstanceKlass::fully_initialized);

    printf("  Class: %s, instance_size=%d\n", ik->class_name(), ik->instance_size());

    // ---- 执行 ----
    JavaThread thread("test");
    JavaValue result(T_INT);

    BytecodeInterpreter::_trace_bytecodes = true;
    BytecodeInterpreter::execute(test_method, ik, &thread, &result);
    BytecodeInterpreter::_trace_bytecodes = false;

    // ---- 验证 ----
    printf("  Result: %d (expected 10)\n", result.get_jint());
    vm_assert(result.get_jint() == 10, "<init> should set count to 10");
    vm_assert(!thread.has_pending_exception(), "no exception");

    // 清理 — test_method 不在 ik->methods() 中，需要手动删除
    delete test_method;
    delete ik;  // 会删除 cp, fields, methods 数组, init_method(通过 methods[0])
    JavaHeap::destroy();

    printf("  [PASS] Phase 6 - <init> Constructor Execution OK\n\n");
}

// ============================================================================
// Phase 6 测试 2：实例方法调用（invokevirtual）
//
// 模拟：
//   class Calculator {
//       int value;
//       Calculator() { this.value = 0; }
//       int addAndGet(int n) { this.value = this.value + n; return this.value; }
//   }
//
// 测试字节码序列（testMethod()I）：
//   new Calculator       → 分配对象
//   dup                  → 复制引用
//   invokespecial <init> → 执行构造函数
//   astore_1             → obj → local[1]
//   aload_1              → push obj
//   bipush 42            → push 42
//   invokevirtual addAndGet → obj.addAndGet(42) → 42
//   ireturn              → 返回 42
//
// <init>()V：
//   aload_0              → this
//   iconst_0             → 0
//   putfield value       → this.value = 0
//   return
//
// addAndGet(I)I：
//   aload_0              → this
//   aload_0              → this
//   getfield value       → this.value
//   iload_1              → n
//   iadd                 → this.value + n
//   putfield value       → this.value = this.value + n
//   aload_0              → this
//   getfield value       → this.value
//   ireturn              → return this.value
// ============================================================================

void test_invokevirtual_instance_method() {
    printf("=== Test: Phase 6 - invokevirtual Instance Method ===\n");

    JavaHeap::initialize(1 * 1024 * 1024);

    // 常量池布局：
    //   #1  = Class        → #2
    //   #2  = UTF8         "test/Calculator"
    //   #3  = UTF8         "value"
    //   #4  = UTF8         "I"
    //   #5  = NameAndType  #3:#4 (value:I)
    //   #6  = Fieldref     #1.#5 (test/Calculator.value:I)
    //   #7  = UTF8         "<init>"
    //   #8  = UTF8         "()V"
    //   #9  = NameAndType  #7:#8 (<init>:()V)
    //   #10 = Methodref    #1.#9 (test/Calculator.<init>:()V)
    //   #11 = UTF8         "addAndGet"
    //   #12 = UTF8         "(I)I"
    //   #13 = NameAndType  #11:#12 (addAndGet:(I)I)
    //   #14 = Methodref    #1.#13 (test/Calculator.addAndGet:(I)I)
    //   #15 = UTF8         "testMethod"
    //   #16 = UTF8         "()I"
    ConstantPool* cp = new ConstantPool(20);

    cp->utf8_at_put(2, (const u1*)"test/Calculator", 15);
    cp->klass_index_at_put(1, 2);

    cp->utf8_at_put(3, (const u1*)"value", 5);
    cp->utf8_at_put(4, (const u1*)"I", 1);
    cp->name_and_type_at_put(5, 3, 4);
    cp->field_at_put(6, 1, 5);

    cp->utf8_at_put(7, (const u1*)"<init>", 6);
    cp->utf8_at_put(8, (const u1*)"()V", 3);
    cp->name_and_type_at_put(9, 7, 8);
    cp->method_at_put(10, 1, 9);

    cp->utf8_at_put(11, (const u1*)"addAndGet", 9);
    cp->utf8_at_put(12, (const u1*)"(I)I", 4);
    cp->name_and_type_at_put(13, 11, 12);
    cp->method_at_put(14, 1, 13);

    cp->utf8_at_put(15, (const u1*)"testMethod", 10);
    cp->utf8_at_put(16, (const u1*)"()I", 3);

    // ---- <init>()V ----
    //   0: aload_0       (2A)
    //   1: iconst_0      (03)
    //   2: putfield #6   (B5 00 06)
    //   5: return        (B1)
    u1 init_code[] = {
        0x2A,                // 0: aload_0
        0x03,                // 1: iconst_0
        0xB5, 0x00, 0x06,    // 2: putfield #6 (value)
        0xB1,                // 5: return
    };
    ConstMethod* init_cm = new ConstMethod(cp, 6, 2, 1, 7, 8);
    init_cm->set_bytecodes(init_code, 6);
    Method* init_method = new Method(init_cm, AccessFlags(JVM_ACC_PUBLIC));

    // ---- addAndGet(I)I ----
    //   0: aload_0       (2A)       → this
    //   1: aload_0       (2A)       → this
    //   2: getfield #6   (B4 00 06) → this.value
    //   5: iload_1       (1B)       → n
    //   6: iadd          (60)       → this.value + n
    //   7: putfield #6   (B5 00 06) → this.value = this.value + n
    //  10: aload_0       (2A)       → this
    //  11: getfield #6   (B4 00 06) → this.value
    //  14: ireturn       (AC)       → return this.value
    u1 add_code[] = {
        0x2A,                // 0: aload_0
        0x2A,                // 1: aload_0
        0xB4, 0x00, 0x06,    // 2: getfield #6 (value)
        0x1B,                // 5: iload_1
        0x60,                // 6: iadd
        0xB5, 0x00, 0x06,    // 7: putfield #6 (value)
        0x2A,                // 10: aload_0
        0xB4, 0x00, 0x06,    // 11: getfield #6 (value)
        0xAC,                // 14: ireturn
    };
    ConstMethod* add_cm = new ConstMethod(cp, 15, 3, 2, 11, 12);
    // max_stack=3 (this + this.value + n), max_locals=2 (this + n)
    add_cm->set_bytecodes(add_code, 15);
    Method* add_method = new Method(add_cm, AccessFlags(JVM_ACC_PUBLIC));

    // ---- testMethod()I ----
    //   0: new #1              (BB 00 01)
    //   3: dup                 (59)
    //   4: invokespecial #10   (B7 00 0A)  → <init>
    //   7: astore_1            (4C)
    //   8: aload_1             (2B)
    //   9: bipush 42           (10 2A)
    //  11: invokevirtual #14   (B6 00 0E)  → addAndGet(42)
    //  14: ireturn             (AC)
    u1 test_code[] = {
        0xBB, 0x00, 0x01,   // 0: new #1
        0x59,                // 3: dup
        0xB7, 0x00, 0x0A,   // 4: invokespecial #10
        0x4C,                // 7: astore_1
        0x2B,                // 8: aload_1
        0x10, 0x2A,          // 9: bipush 42
        0xB6, 0x00, 0x0E,   // 11: invokevirtual #14
        0xAC,                // 14: ireturn
    };
    ConstMethod* test_cm = new ConstMethod(cp, 15, 4, 2, 15, 16);
    test_cm->set_bytecodes(test_code, 15);
    Method* test_method = new Method(test_cm, AccessFlags(JVM_ACC_PUBLIC | JVM_ACC_STATIC));

    // ---- 字段 ----
    FieldInfoEntry* fields = NEW_C_HEAP_ARRAY(FieldInfoEntry, 1, mtClass);
    fields[0] = { 0, 3, 4, (u2)instanceOopDesc::base_offset_in_bytes(), 0 }; // value at offset 16

    // ---- 方法数组（<init> + addAndGet）----
    Method** methods_arr = NEW_C_HEAP_ARRAY(Method*, 2, mtClass);
    methods_arr[0] = init_method;
    methods_arr[1] = add_method;

    // ---- InstanceKlass ----
    InstanceKlass* ik = new InstanceKlass();
    ik->set_class_name("test/Calculator");
    ik->set_constants(cp);
    ik->set_fields(fields, 1);
    ik->set_methods(methods_arr, 2);
    ik->set_has_nonstatic_fields();

    int inst_size = (int)align_up((uintx)(instanceOopDesc::base_offset_in_bytes() + 4), (uintx)HeapWordSize);
    ik->set_instance_size(inst_size);
    ik->set_init_state(InstanceKlass::fully_initialized);

    printf("  Class: %s, instance_size=%d\n", ik->class_name(), ik->instance_size());
    printf("  Methods: <init>()V, addAndGet(I)I\n");

    // ---- 执行 ----
    JavaThread thread("test");
    JavaValue result(T_INT);

    BytecodeInterpreter::_trace_bytecodes = true;
    BytecodeInterpreter::execute(test_method, ik, &thread, &result);
    BytecodeInterpreter::_trace_bytecodes = false;

    // ---- 验证 ----
    printf("  Result: %d (expected 42)\n", result.get_jint());
    vm_assert(result.get_jint() == 42, "addAndGet(42) should return 42");
    vm_assert(!thread.has_pending_exception(), "no exception");

    // 清理
    delete test_method;
    delete ik;
    JavaHeap::destroy();

    printf("  [PASS] Phase 6 - invokevirtual Instance Method OK\n\n");
}

// ============================================================================
// Phase 6 测试 3：多次实例方法调用（累加）
//
// 模拟：
//   class Accumulator {
//       int sum;
//       Accumulator() { this.sum = 0; }
//       int add(int n) { this.sum = this.sum + n; return this.sum; }
//   }
//
// 测试：创建对象 → add(10) → add(20) → add(30) → return sum
// 期望结果：60
// ============================================================================

void test_multiple_method_calls() {
    printf("=== Test: Phase 6 - Multiple Method Calls (Accumulator) ===\n");

    JavaHeap::initialize(1 * 1024 * 1024);

    // 常量池
    ConstantPool* cp = new ConstantPool(20);

    cp->utf8_at_put(2, (const u1*)"test/Accumulator", 16);
    cp->klass_index_at_put(1, 2);

    cp->utf8_at_put(3, (const u1*)"sum", 3);
    cp->utf8_at_put(4, (const u1*)"I", 1);
    cp->name_and_type_at_put(5, 3, 4);
    cp->field_at_put(6, 1, 5);

    cp->utf8_at_put(7, (const u1*)"<init>", 6);
    cp->utf8_at_put(8, (const u1*)"()V", 3);
    cp->name_and_type_at_put(9, 7, 8);
    cp->method_at_put(10, 1, 9);

    cp->utf8_at_put(11, (const u1*)"add", 3);
    cp->utf8_at_put(12, (const u1*)"(I)I", 4);
    cp->name_and_type_at_put(13, 11, 12);
    cp->method_at_put(14, 1, 13);

    cp->utf8_at_put(15, (const u1*)"testMethod", 10);
    cp->utf8_at_put(16, (const u1*)"()I", 3);

    // ---- <init>()V ----
    u1 init_code[] = {
        0x2A,                // aload_0
        0x03,                // iconst_0
        0xB5, 0x00, 0x06,    // putfield #6 (sum)
        0xB1,                // return
    };
    ConstMethod* init_cm = new ConstMethod(cp, 6, 2, 1, 7, 8);
    init_cm->set_bytecodes(init_code, 6);
    Method* init_method = new Method(init_cm, AccessFlags(JVM_ACC_PUBLIC));

    // ---- add(I)I ----
    //   this.sum = this.sum + n; return this.sum;
    u1 add_code[] = {
        0x2A,                // 0: aload_0
        0x2A,                // 1: aload_0
        0xB4, 0x00, 0x06,    // 2: getfield #6 (sum)
        0x1B,                // 5: iload_1
        0x60,                // 6: iadd
        0xB5, 0x00, 0x06,    // 7: putfield #6 (sum)
        0x2A,                // 10: aload_0
        0xB4, 0x00, 0x06,    // 11: getfield #6 (sum)
        0xAC,                // 14: ireturn
    };
    ConstMethod* add_cm = new ConstMethod(cp, 15, 3, 2, 11, 12);
    add_cm->set_bytecodes(add_code, 15);
    Method* add_method = new Method(add_cm, AccessFlags(JVM_ACC_PUBLIC));

    // ---- testMethod()I ----
    //   new → dup → <init> → astore_1
    //   aload_1 → bipush 10 → invokevirtual add → pop (丢弃返回值)
    //   aload_1 → bipush 20 → invokevirtual add → pop
    //   aload_1 → bipush 30 → invokevirtual add → ireturn (返回最后一次 add 的结果)
    u1 test_code[] = {
        0xBB, 0x00, 0x01,   //  0: new #1
        0x59,                //  3: dup
        0xB7, 0x00, 0x0A,   //  4: invokespecial #10 (<init>)
        0x4C,                //  7: astore_1
        // add(10) → pop
        0x2B,                //  8: aload_1
        0x10, 0x0A,          //  9: bipush 10
        0xB6, 0x00, 0x0E,   // 11: invokevirtual #14 (add)
        0x57,                // 14: pop
        // add(20) → pop
        0x2B,                // 15: aload_1
        0x10, 0x14,          // 16: bipush 20
        0xB6, 0x00, 0x0E,   // 18: invokevirtual #14 (add)
        0x57,                // 21: pop
        // add(30) → return result
        0x2B,                // 22: aload_1
        0x10, 0x1E,          // 23: bipush 30
        0xB6, 0x00, 0x0E,   // 25: invokevirtual #14 (add)
        0xAC,                // 28: ireturn
    };
    ConstMethod* test_cm = new ConstMethod(cp, 29, 4, 2, 15, 16);
    test_cm->set_bytecodes(test_code, 29);
    Method* test_method = new Method(test_cm, AccessFlags(JVM_ACC_PUBLIC | JVM_ACC_STATIC));

    // ---- 字段 ----
    FieldInfoEntry* fields = NEW_C_HEAP_ARRAY(FieldInfoEntry, 1, mtClass);
    fields[0] = { 0, 3, 4, (u2)instanceOopDesc::base_offset_in_bytes(), 0 };

    // ---- 方法 ----
    Method** methods_arr = NEW_C_HEAP_ARRAY(Method*, 2, mtClass);
    methods_arr[0] = init_method;
    methods_arr[1] = add_method;

    // ---- InstanceKlass ----
    InstanceKlass* ik = new InstanceKlass();
    ik->set_class_name("test/Accumulator");
    ik->set_constants(cp);
    ik->set_fields(fields, 1);
    ik->set_methods(methods_arr, 2);
    ik->set_has_nonstatic_fields();

    int inst_size = (int)align_up((uintx)(instanceOopDesc::base_offset_in_bytes() + 4), (uintx)HeapWordSize);
    ik->set_instance_size(inst_size);
    ik->set_init_state(InstanceKlass::fully_initialized);

    printf("  Class: %s\n", ik->class_name());
    printf("  Testing: new → <init> → add(10) → add(20) → add(30)\n");

    // ---- 执行 ----
    JavaThread thread("test");
    JavaValue result(T_INT);

    BytecodeInterpreter::_trace_bytecodes = false; // 关闭 trace，输出太长
    BytecodeInterpreter::execute(test_method, ik, &thread, &result);

    // ---- 验证 ----
    printf("  Result: %d (expected 60)\n", result.get_jint());
    vm_assert(result.get_jint() == 60, "10 + 20 + 30 = 60");
    vm_assert(!thread.has_pending_exception(), "no exception");

    delete test_method;
    delete ik;
    JavaHeap::destroy();

    printf("  [PASS] Phase 6 - Multiple Method Calls OK\n\n");
}

// ============================================================================
// Phase 6 测试 4：<init> 带参数的构造函数
//
// 模拟：
//   class Point {
//       int x; int y;
//       Point(int x, int y) { this.x = x; this.y = y; }
//       int sum() { return this.x + this.y; }
//   }
//
// 测试：new Point → dup → invokespecial <init>(3, 7) → sum() → return 10
// ============================================================================

void test_init_with_args() {
    printf("=== Test: Phase 6 - <init> With Arguments ===\n");

    JavaHeap::initialize(1 * 1024 * 1024);

    // 常量池
    ConstantPool* cp = new ConstantPool(25);

    cp->utf8_at_put(2, (const u1*)"test/Point", 10);
    cp->klass_index_at_put(1, 2);

    // 字段
    cp->utf8_at_put(3, (const u1*)"x", 1);
    cp->utf8_at_put(4, (const u1*)"I", 1);
    cp->name_and_type_at_put(5, 3, 4);      // x:I
    cp->field_at_put(6, 1, 5);              // Fieldref Point.x:I

    cp->utf8_at_put(7, (const u1*)"y", 1);
    // descriptor "I" 复用 #4
    cp->name_and_type_at_put(8, 7, 4);      // y:I
    cp->field_at_put(9, 1, 8);              // Fieldref Point.y:I

    // <init>(II)V
    cp->utf8_at_put(10, (const u1*)"<init>", 6);
    cp->utf8_at_put(11, (const u1*)"(II)V", 5);
    cp->name_and_type_at_put(12, 10, 11);
    cp->method_at_put(13, 1, 12);           // Methodref Point.<init>:(II)V

    // sum()I
    cp->utf8_at_put(14, (const u1*)"sum", 3);
    cp->utf8_at_put(15, (const u1*)"()I", 3);
    cp->name_and_type_at_put(16, 14, 15);
    cp->method_at_put(17, 1, 16);           // Methodref Point.sum:()I

    // testMethod
    cp->utf8_at_put(18, (const u1*)"testMethod", 10);
    cp->utf8_at_put(19, (const u1*)"()I", 3);

    // ---- <init>(II)V ----
    //   aload_0 → iload_1 → putfield x → aload_0 → iload_2 → putfield y → return
    u1 init_code[] = {
        0x2A,                // 0: aload_0  (this)
        0x1B,                // 1: iload_1  (x)
        0xB5, 0x00, 0x06,    // 2: putfield #6 (this.x = x)
        0x2A,                // 5: aload_0  (this)
        0x1C,                // 6: iload_2  (y)
        0xB5, 0x00, 0x09,    // 7: putfield #9 (this.y = y)
        0xB1,                // 10: return
    };
    ConstMethod* init_cm = new ConstMethod(cp, 11, 3, 3, 10, 11);
    // max_stack=3 (this + value + ...), max_locals=3 (this, x, y)
    init_cm->set_bytecodes(init_code, 11);
    Method* init_method = new Method(init_cm, AccessFlags(JVM_ACC_PUBLIC));

    // ---- sum()I ----
    //   aload_0 → getfield x → aload_0 → getfield y → iadd → ireturn
    u1 sum_code[] = {
        0x2A,                // 0: aload_0
        0xB4, 0x00, 0x06,    // 1: getfield #6 (x)
        0x2A,                // 4: aload_0
        0xB4, 0x00, 0x09,    // 5: getfield #9 (y)
        0x60,                // 8: iadd
        0xAC,                // 9: ireturn
    };
    ConstMethod* sum_cm = new ConstMethod(cp, 10, 3, 1, 14, 15);
    sum_cm->set_bytecodes(sum_code, 10);
    Method* sum_method = new Method(sum_cm, AccessFlags(JVM_ACC_PUBLIC));

    // ---- testMethod()I ----
    //   new Point → dup → iconst_3 → bipush 7 → invokespecial <init>(3,7)
    //   → astore_1 → aload_1 → invokevirtual sum() → ireturn
    u1 test_code[] = {
        0xBB, 0x00, 0x01,   //  0: new #1 (Point)
        0x59,                //  3: dup
        0x06,                //  4: iconst_3
        0x10, 0x07,          //  5: bipush 7
        0xB7, 0x00, 0x0D,   //  7: invokespecial #13 (<init>(II)V)
        0x4C,                // 10: astore_1
        0x2B,                // 11: aload_1
        0xB6, 0x00, 0x11,   // 12: invokevirtual #17 (sum()I)
        0xAC,                // 15: ireturn
    };
    ConstMethod* test_cm = new ConstMethod(cp, 16, 5, 2, 18, 19);
    test_cm->set_bytecodes(test_code, 16);
    Method* test_method = new Method(test_cm, AccessFlags(JVM_ACC_PUBLIC | JVM_ACC_STATIC));

    // ---- 字段 ----
    FieldInfoEntry* fields = NEW_C_HEAP_ARRAY(FieldInfoEntry, 2, mtClass);
    fields[0] = { 0, 3, 4, (u2)instanceOopDesc::base_offset_in_bytes(), 0 };      // x at 16
    fields[1] = { 0, 7, 4, (u2)(instanceOopDesc::base_offset_in_bytes() + 4), 0 }; // y at 20

    // ---- 方法 ----
    Method** methods_arr = NEW_C_HEAP_ARRAY(Method*, 2, mtClass);
    methods_arr[0] = init_method;
    methods_arr[1] = sum_method;

    // ---- InstanceKlass ----
    InstanceKlass* ik = new InstanceKlass();
    ik->set_class_name("test/Point");
    ik->set_constants(cp);
    ik->set_fields(fields, 2);
    ik->set_methods(methods_arr, 2);
    ik->set_has_nonstatic_fields();

    int inst_size = (int)align_up((uintx)(instanceOopDesc::base_offset_in_bytes() + 8), (uintx)HeapWordSize);
    ik->set_instance_size(inst_size);
    ik->set_init_state(InstanceKlass::fully_initialized);

    printf("  Class: %s, instance_size=%d\n", ik->class_name(), ik->instance_size());
    printf("  Testing: new Point(3, 7) → sum() → expect 10\n");

    // ---- 执行 ----
    JavaThread thread("test");
    JavaValue result(T_INT);

    BytecodeInterpreter::_trace_bytecodes = true;
    BytecodeInterpreter::execute(test_method, ik, &thread, &result);
    BytecodeInterpreter::_trace_bytecodes = false;

    // ---- 验证 ----
    printf("  Result: %d (expected 10)\n", result.get_jint());
    vm_assert(result.get_jint() == 10, "Point(3,7).sum() should be 10");
    vm_assert(!thread.has_pending_exception(), "no exception");

    delete test_method;
    delete ik;
    JavaHeap::destroy();

    printf("  [PASS] Phase 6 - <init> With Arguments OK\n\n");
}

// ============================================================================
// Phase 7 测试：数组支持
// ============================================================================

// ---- Test 1: TypeArrayKlass 基础 + 直接 API ----
// 验证 TypeArrayKlass 初始化、数组分配、元素读写
void test_type_array_klass_basic() {
    printf("=== Test: Phase 7 - TypeArrayKlass Basic ===\n");

    JavaHeap::initialize(1024 * 1024);  // 1MB
    TypeArrayKlass::initialize_all();

    // 检查各种类型的 TypeArrayKlass 都存在
    vm_assert(TypeArrayKlass::for_type(T_INT)     != nullptr, "int[] klass");
    vm_assert(TypeArrayKlass::for_type(T_BYTE)    != nullptr, "byte[] klass");
    vm_assert(TypeArrayKlass::for_type(T_CHAR)    != nullptr, "char[] klass");
    vm_assert(TypeArrayKlass::for_type(T_LONG)    != nullptr, "long[] klass");
    vm_assert(TypeArrayKlass::for_type(T_FLOAT)   != nullptr, "float[] klass");
    vm_assert(TypeArrayKlass::for_type(T_DOUBLE)  != nullptr, "double[] klass");
    vm_assert(TypeArrayKlass::for_type(T_SHORT)   != nullptr, "short[] klass");
    vm_assert(TypeArrayKlass::for_type(T_BOOLEAN) != nullptr, "boolean[] klass");

    // 分配 int[5]
    TypeArrayKlass* int_klass = TypeArrayKlass::for_type(T_INT);
    printf("  int[] klass: name=%s, element_size=%d\n",
           int_klass->name(), int_klass->element_size());

    typeArrayOopDesc* arr = int_klass->allocate_array(5);
    vm_assert(arr != nullptr, "array allocated");
    vm_assert(arr->length() == 5, "length should be 5");
    vm_assert(arr->klass() == int_klass, "klass should match");

    // 写入并读回
    arr->int_at_put(0, 10);
    arr->int_at_put(1, 20);
    arr->int_at_put(2, 30);
    arr->int_at_put(3, 40);
    arr->int_at_put(4, 50);

    vm_assert(arr->int_at(0) == 10, "arr[0]=10");
    vm_assert(arr->int_at(4) == 50, "arr[4]=50");

    // 验证数组大小
    // int[5]: header(24) + 5*4(20) = 44 → aligned to 48
    int expected_size = int_klass->array_size_in_bytes(5);
    printf("  int[5] size: %d bytes (header=%d, data=%d)\n",
           expected_size, arrayOopDesc::header_size_in_bytes(), 5 * 4);
    vm_assert(expected_size == 48, "int[5] should be 48 bytes");

    // 分配 byte[10]
    TypeArrayKlass* byte_klass = TypeArrayKlass::for_type(T_BYTE);
    typeArrayOopDesc* barr = byte_klass->allocate_array(10);
    vm_assert(barr != nullptr, "byte array allocated");
    vm_assert(barr->length() == 10, "byte array length 10");

    barr->byte_at_put(0, 'H');
    barr->byte_at_put(1, 'i');
    vm_assert(barr->byte_at(0) == 'H', "barr[0]='H'");
    vm_assert(barr->byte_at(1) == 'i', "barr[1]='i'");

    TypeArrayKlass::destroy_all();
    JavaHeap::destroy();

    printf("  [PASS] Phase 7 - TypeArrayKlass Basic OK\n\n");
}

// ---- Test 2: newarray + iastore + iaload 字节码 ----
// Java 等价代码：
//   int[] arr = new int[3];
//   arr[0] = 10;
//   arr[1] = 20;
//   arr[2] = 30;
//   return arr[0] + arr[1] + arr[2];  // → 60
void test_newarray_int() {
    printf("=== Test: Phase 7 - newarray int + iastore/iaload ===\n");

    JavaHeap::initialize(1024 * 1024);
    TypeArrayKlass::initialize_all();

    // 构建字节码：
    // 0: iconst_3
    // 1: newarray T_INT(10)
    // 3: astore_1
    //
    // 4: aload_1
    // 5: iconst_0
    // 6: bipush 10
    // 8: iastore
    //
    // 9: aload_1
    // 10: iconst_1
    // 11: bipush 20
    // 13: iastore
    //
    // 14: aload_1
    // 15: iconst_2
    // 16: bipush 30
    // 18: iastore
    //
    // 19: aload_1
    // 20: iconst_0
    // 21: iaload
    //
    // 22: aload_1
    // 23: iconst_1
    // 24: iaload
    //
    // 25: iadd
    //
    // 26: aload_1
    // 27: iconst_2
    // 28: iaload
    //
    // 29: iadd
    //
    // 30: ireturn

    u1 bytecodes[] = {
        0x06,                         //  0: iconst_3
        (u1)Bytecodes::_newarray, 10, //  1: newarray T_INT
        (u1)Bytecodes::_astore_1,     //  3: astore_1

        (u1)Bytecodes::_aload_1,      //  4: aload_1
        (u1)Bytecodes::_iconst_0,     //  5: iconst_0
        (u1)Bytecodes::_bipush, 10,   //  6: bipush 10
        (u1)Bytecodes::_iastore,      //  8: iastore

        (u1)Bytecodes::_aload_1,      //  9: aload_1
        (u1)Bytecodes::_iconst_1,     // 10: iconst_1
        (u1)Bytecodes::_bipush, 20,   // 11: bipush 20
        (u1)Bytecodes::_iastore,      // 13: iastore

        (u1)Bytecodes::_aload_1,      // 14: aload_1
        (u1)Bytecodes::_iconst_2,     // 15: iconst_2
        (u1)Bytecodes::_bipush, 30,   // 16: bipush 30
        (u1)Bytecodes::_iastore,      // 18: iastore

        (u1)Bytecodes::_aload_1,      // 19: aload_1
        (u1)Bytecodes::_iconst_0,     // 20: iconst_0
        (u1)Bytecodes::_iaload,       // 21: iaload

        (u1)Bytecodes::_aload_1,      // 22: aload_1
        (u1)Bytecodes::_iconst_1,     // 23: iconst_1
        (u1)Bytecodes::_iaload,       // 24: iaload

        (u1)Bytecodes::_iadd,         // 25: iadd

        (u1)Bytecodes::_aload_1,      // 26: aload_1
        (u1)Bytecodes::_iconst_2,     // 27: iconst_2
        (u1)Bytecodes::_iaload,       // 28: iaload

        (u1)Bytecodes::_iadd,         // 29: iadd
        (u1)Bytecodes::_ireturn,      // 30: ireturn
    };

    int code_length = sizeof(bytecodes);

    // 创建 Method（不需要常量池，数组字节码不用 cp）
    ConstantPool* cp = new ConstantPool(1);
    ConstMethod* cm = new ConstMethod(cp, code_length, 4, 2, 0, 0);
    cm->set_bytecodes(bytecodes, code_length);
    Method* method = new Method(cm, AccessFlags(JVM_ACC_PUBLIC | JVM_ACC_STATIC));

    // 创建一个最小的 InstanceKlass（只用于提供 cp）
    InstanceKlass* ik = new InstanceKlass();
    ik->set_name("test/ArrayTest");
    ik->set_constants(cp);
    ik->set_init_state(InstanceKlass::fully_initialized);

    // 执行
    JavaThread thread("test");
    JavaValue result(T_INT);

    BytecodeInterpreter::_trace_bytecodes = true;
    BytecodeInterpreter::execute(method, ik, &thread, &result);
    BytecodeInterpreter::_trace_bytecodes = false;

    printf("  Result: %d (expected 60)\n", result.get_jint());
    vm_assert(result.get_jint() == 60, "arr[0]+arr[1]+arr[2] should be 60");
    vm_assert(!thread.has_pending_exception(), "no exception");

    delete method;
    delete ik;
    TypeArrayKlass::destroy_all();
    JavaHeap::destroy();

    printf("  [PASS] Phase 7 - newarray int + iastore/iaload OK\n\n");
}

// ---- Test 3: arraylength 字节码 ----
// Java 等价代码：
//   int[] arr = new int[7];
//   return arr.length;  // → 7
void test_arraylength() {
    printf("=== Test: Phase 7 - arraylength ===\n");

    JavaHeap::initialize(1024 * 1024);
    TypeArrayKlass::initialize_all();

    // 0: bipush 7
    // 2: newarray T_INT(10)
    // 4: arraylength
    // 5: ireturn
    u1 bytecodes[] = {
        (u1)Bytecodes::_bipush, 7,
        (u1)Bytecodes::_newarray, 10,  // T_INT
        (u1)Bytecodes::_arraylength,
        (u1)Bytecodes::_ireturn,
    };

    ConstantPool* cp = new ConstantPool(1);
    ConstMethod* cm = new ConstMethod(cp, sizeof(bytecodes), 2, 1, 0, 0);
    cm->set_bytecodes(bytecodes, sizeof(bytecodes));
    Method* method = new Method(cm, AccessFlags(JVM_ACC_PUBLIC | JVM_ACC_STATIC));

    InstanceKlass* ik = new InstanceKlass();
    ik->set_name("test/ArrayLen");
    ik->set_constants(cp);
    ik->set_init_state(InstanceKlass::fully_initialized);

    JavaThread thread("test");
    JavaValue result(T_INT);

    BytecodeInterpreter::_trace_bytecodes = true;
    BytecodeInterpreter::execute(method, ik, &thread, &result);
    BytecodeInterpreter::_trace_bytecodes = false;

    printf("  Result: %d (expected 7)\n", result.get_jint());
    vm_assert(result.get_jint() == 7, "arraylength should be 7");

    delete method;
    delete ik;
    TypeArrayKlass::destroy_all();
    JavaHeap::destroy();

    printf("  [PASS] Phase 7 - arraylength OK\n\n");
}

// ---- Test 4: byte 数组 (bastore/baload) ----
// Java 等价代码：
//   byte[] arr = new byte[3];
//   arr[0] = 100;
//   arr[1] = 50;
//   arr[2] = -10;
//   return arr[0] + arr[1] + arr[2];  // → 140
void test_byte_array() {
    printf("=== Test: Phase 7 - byte array (bastore/baload) ===\n");

    JavaHeap::initialize(1024 * 1024);
    TypeArrayKlass::initialize_all();

    // 0: iconst_3
    // 1: newarray T_BYTE(8)
    // 3: astore_1
    //
    // 4: aload_1
    // 5: iconst_0
    // 6: bipush 100
    // 8: bastore
    //
    // 9: aload_1
    // 10: iconst_1
    // 11: bipush 50
    // 13: bastore
    //
    // 14: aload_1
    // 15: iconst_2
    // 16: bipush -10   (0xF6 as signed byte)
    // 18: bastore
    //
    // 19: aload_1
    // 20: iconst_0
    // 21: baload
    //
    // 22: aload_1
    // 23: iconst_1
    // 24: baload
    //
    // 25: iadd
    //
    // 26: aload_1
    // 27: iconst_2
    // 28: baload
    //
    // 29: iadd
    //
    // 30: ireturn

    u1 bytecodes[] = {
        0x06,                         //  0: iconst_3
        (u1)Bytecodes::_newarray, 8,  //  1: newarray T_BYTE
        (u1)Bytecodes::_astore_1,     //  3: astore_1

        (u1)Bytecodes::_aload_1,      //  4
        (u1)Bytecodes::_iconst_0,     //  5
        (u1)Bytecodes::_bipush, 100,  //  6
        (u1)Bytecodes::_bastore,      //  8

        (u1)Bytecodes::_aload_1,      //  9
        (u1)Bytecodes::_iconst_1,     // 10
        (u1)Bytecodes::_bipush, 50,   // 11
        (u1)Bytecodes::_bastore,      // 13

        (u1)Bytecodes::_aload_1,      // 14
        (u1)Bytecodes::_iconst_2,     // 15
        (u1)Bytecodes::_bipush, (u1)(jbyte)-10,  // 16: bipush -10
        (u1)Bytecodes::_bastore,      // 18

        (u1)Bytecodes::_aload_1,      // 19
        (u1)Bytecodes::_iconst_0,     // 20
        (u1)Bytecodes::_baload,       // 21

        (u1)Bytecodes::_aload_1,      // 22
        (u1)Bytecodes::_iconst_1,     // 23
        (u1)Bytecodes::_baload,       // 24

        (u1)Bytecodes::_iadd,         // 25

        (u1)Bytecodes::_aload_1,      // 26
        (u1)Bytecodes::_iconst_2,     // 27
        (u1)Bytecodes::_baload,       // 28

        (u1)Bytecodes::_iadd,         // 29
        (u1)Bytecodes::_ireturn,      // 30
    };

    ConstantPool* cp = new ConstantPool(1);
    ConstMethod* cm = new ConstMethod(cp, sizeof(bytecodes), 4, 2, 0, 0);
    cm->set_bytecodes(bytecodes, sizeof(bytecodes));
    Method* method = new Method(cm, AccessFlags(JVM_ACC_PUBLIC | JVM_ACC_STATIC));

    InstanceKlass* ik = new InstanceKlass();
    ik->set_name("test/ByteArrayTest");
    ik->set_constants(cp);
    ik->set_init_state(InstanceKlass::fully_initialized);

    JavaThread thread("test");
    JavaValue result(T_INT);

    BytecodeInterpreter::_trace_bytecodes = true;
    BytecodeInterpreter::execute(method, ik, &thread, &result);
    BytecodeInterpreter::_trace_bytecodes = false;

    // 100 + 50 + (-10) = 140
    printf("  Result: %d (expected 140)\n", result.get_jint());
    vm_assert(result.get_jint() == 140, "100+50+(-10) should be 140");

    delete method;
    delete ik;
    TypeArrayKlass::destroy_all();
    JavaHeap::destroy();

    printf("  [PASS] Phase 7 - byte array OK\n\n");
}

// ---- Test 5: 数组用于循环累加 ----
// Java 等价代码：
//   int[] arr = new int[4];
//   arr[0] = 1; arr[1] = 2; arr[2] = 3; arr[3] = 4;
//   int sum = 0;
//   int i = 0;
//   while (i < 4) { sum += arr[i]; i++; }
//   return sum;  // → 10
void test_array_loop_sum() {
    printf("=== Test: Phase 7 - Array Loop Sum ===\n");

    JavaHeap::initialize(1024 * 1024);
    TypeArrayKlass::initialize_all();

    // local[0] = (unused)
    // local[1] = arr (int[4])
    // local[2] = sum
    // local[3] = i
    //
    // Bytecode:
    //  0: iconst_4
    //  1: newarray 10 (T_INT)
    //  3: astore_1
    //
    //  4: aload_1 / iconst_0 / iconst_1 / iastore     (arr[0]=1)
    //  8: aload_1 / iconst_1 / iconst_2 / iastore     (arr[1]=2)
    // 12: aload_1 / iconst_2 / iconst_3 / iastore     (arr[2]=3)
    // 16: aload_1 / iconst_3 / iconst_4 / iastore     (arr[3]=4)
    //
    // 20: iconst_0 / istore_2                          (sum=0)
    // 22: iconst_0 / istore_3                          (i=0)
    //
    // 24: iload_3                                      (loop start)
    // 25: iconst_4
    // 26: if_icmpge +14 → 40                           (if i>=4 goto end)
    //
    // 29: iload_2                                      (sum)
    // 30: aload_1                                      (arr)
    // 31: iload_3                                      (i)
    // 32: iaload                                       (arr[i])
    // 33: iadd                                         (sum + arr[i])
    // 34: istore_2                                     (sum = ...)
    //
    // 35: iinc 3 1                                     (i++)
    // 38: goto -14 → 24                                (goto loop)
    //
    // 40: iload_2                                      (load sum)
    // 41: ireturn

    u1 bytecodes[] = {
        // 创建数组
        0x07,                                   //  0: iconst_4
        (u1)Bytecodes::_newarray, 10,           //  1: newarray T_INT
        (u1)Bytecodes::_astore_1,               //  3: astore_1

        // arr[0]=1
        (u1)Bytecodes::_aload_1,                //  4: aload_1
        (u1)Bytecodes::_iconst_0,               //  5: iconst_0
        (u1)Bytecodes::_iconst_1,               //  6: iconst_1
        (u1)Bytecodes::_iastore,                //  7: iastore

        // arr[1]=2
        (u1)Bytecodes::_aload_1,                //  8: aload_1
        (u1)Bytecodes::_iconst_1,               //  9: iconst_1
        (u1)Bytecodes::_iconst_2,               // 10: iconst_2
        (u1)Bytecodes::_iastore,                // 11: iastore

        // arr[2]=3
        (u1)Bytecodes::_aload_1,                // 12: aload_1
        (u1)Bytecodes::_iconst_2,               // 13: iconst_2
        (u1)Bytecodes::_iconst_3,               // 14: iconst_3
        (u1)Bytecodes::_iastore,                // 15: iastore

        // arr[3]=4
        (u1)Bytecodes::_aload_1,                // 16: aload_1
        (u1)Bytecodes::_iconst_3,               // 17: iconst_3
        (u1)Bytecodes::_iconst_4,               // 18: iconst_4
        (u1)Bytecodes::_iastore,                // 19: iastore

        // sum=0
        (u1)Bytecodes::_iconst_0,               // 20: iconst_0
        (u1)Bytecodes::_istore_2,               // 21: istore_2

        // i=0
        (u1)Bytecodes::_iconst_0,               // 22: iconst_0
        (u1)Bytecodes::_istore_3,               // 23: istore_3

        // loop: if (i >= 4) goto end (BCI 41)
        (u1)Bytecodes::_iload_3,                // 24: iload_3
        (u1)Bytecodes::_iconst_4,               // 25: iconst_4
        (u1)Bytecodes::_if_icmpge, 0x00, 15,    // 26: if_icmpge +15 → 41

        // sum += arr[i]
        (u1)Bytecodes::_iload_2,                // 29: iload_2
        (u1)Bytecodes::_aload_1,                // 30: aload_1
        (u1)Bytecodes::_iload_3,                // 31: iload_3
        (u1)Bytecodes::_iaload,                 // 32: iaload
        (u1)Bytecodes::_iadd,                   // 33: iadd
        (u1)Bytecodes::_istore_2,               // 34: istore_2

        // i++
        (u1)Bytecodes::_iinc, 3, 1,             // 35: iinc 3 1

        // goto loop (BCI 24)
        (u1)Bytecodes::_goto, 0xFF, 0xF2,       // 38: goto -14 → 24

        // end
        (u1)Bytecodes::_iload_2,                // 41: iload_2
        (u1)Bytecodes::_ireturn,                // 42: ireturn
    };

    ConstantPool* cp = new ConstantPool(1);
    ConstMethod* cm = new ConstMethod(cp, sizeof(bytecodes), 4, 4, 0, 0);
    cm->set_bytecodes(bytecodes, sizeof(bytecodes));
    Method* method = new Method(cm, AccessFlags(JVM_ACC_PUBLIC | JVM_ACC_STATIC));

    InstanceKlass* ik = new InstanceKlass();
    ik->set_name("test/ArrayLoop");
    ik->set_constants(cp);
    ik->set_init_state(InstanceKlass::fully_initialized);

    JavaThread thread("test");
    JavaValue result(T_INT);

    BytecodeInterpreter::_trace_bytecodes = false;  // 循环太长，不打印
    BytecodeInterpreter::execute(method, ik, &thread, &result);

    printf("  Result: %d (expected 10)\n", result.get_jint());
    vm_assert(result.get_jint() == 10, "sum should be 1+2+3+4=10");
    vm_assert(!thread.has_pending_exception(), "no exception");

    delete method;
    delete ik;
    TypeArrayKlass::destroy_all();
    JavaHeap::destroy();

    printf("  [PASS] Phase 7 - Array Loop Sum OK\n\n");
}

// ============================================================================
// Phase 8 测试：引用比较、Switch、Long 操作、pop2
// ============================================================================

// ---- Test 28: if_acmpeq / if_acmpne ----
// Java 等价代码：
//   Object a = new int[1];
//   Object b = a;
//   if (a == b) return 1; else return 0;
// 期望结果：1
void test_if_acmpeq() {
    printf("=== Test: Phase 8 - if_acmpeq (reference comparison) ===\n");

    JavaHeap::initialize(1024 * 1024);
    TypeArrayKlass::initialize_all();

    // 字节码：
    // 0: iconst_1
    // 1: newarray T_INT(10)    → 创建 int[1]
    // 3: astore_0              → a = arrayref
    // 4: aload_0               → push a
    // 5: astore_1              → b = a
    // 6: aload_0               → push a
    // 7: aload_1               → push b
    // 8: if_acmpeq +5          → if (a == b) goto 13
    // 11: iconst_0
    // 12: ireturn
    // 13: iconst_1
    // 14: ireturn

    u1 bytecodes[] = {
        (u1)Bytecodes::_iconst_1,                  //  0: iconst_1
        (u1)Bytecodes::_newarray, 10,              //  1: newarray T_INT
        (u1)Bytecodes::_astore_0,                  //  3: astore_0
        (u1)Bytecodes::_aload_0,                   //  4: aload_0
        (u1)Bytecodes::_astore_1,                  //  5: astore_1
        (u1)Bytecodes::_aload_0,                   //  6: aload_0
        (u1)Bytecodes::_aload_1,                   //  7: aload_1
        (u1)Bytecodes::_if_acmpeq, 0x00, 0x05,    //  8: if_acmpeq +5 → BCI 13
        (u1)Bytecodes::_iconst_0,                  // 11: iconst_0
        (u1)Bytecodes::_ireturn,                   // 12: ireturn
        (u1)Bytecodes::_iconst_1,                  // 13: iconst_1
        (u1)Bytecodes::_ireturn,                   // 14: ireturn
    };

    int code_length = sizeof(bytecodes);
    ConstantPool* cp = new ConstantPool(1);
    ConstMethod* cm = new ConstMethod(cp, code_length, 4, 2, 0, 0);
    cm->set_bytecodes(bytecodes, code_length);
    Method* method = new Method(cm, AccessFlags(JVM_ACC_PUBLIC | JVM_ACC_STATIC));

    InstanceKlass* ik = new InstanceKlass();
    ik->set_name("test/RefCmp");
    ik->set_constants(cp);
    ik->set_init_state(InstanceKlass::fully_initialized);

    JavaThread thread("test");
    JavaValue result(T_INT);

    BytecodeInterpreter::_trace_bytecodes = true;
    BytecodeInterpreter::execute(method, ik, &thread, &result);
    BytecodeInterpreter::_trace_bytecodes = false;

    printf("  Result: %d (expected 1)\n", result.get_jint());
    vm_assert(result.get_jint() == 1, "same reference should be equal");
    vm_assert(!thread.has_pending_exception(), "no exception");

    delete method;
    delete ik;
    TypeArrayKlass::destroy_all();
    JavaHeap::destroy();

    printf("  [PASS] Phase 8 - if_acmpeq OK\n\n");
}

// ---- Test 29: ifnull / ifnonnull ----
// Java 等价代码：
//   Object a = null;
//   if (a == null) return 10; else return 20;
// 期望结果：10
void test_ifnull() {
    printf("=== Test: Phase 8 - ifnull / ifnonnull ===\n");

    // 字节码：
    // 0: aconst_null
    // 1: ifnull +6              → if null, goto 7
    // 4: bipush 20
    // 6: ireturn
    // 7: bipush 10
    // 9: ireturn

    u1 bytecodes[] = {
        (u1)Bytecodes::_aconst_null,               //  0: aconst_null
        (u1)Bytecodes::_ifnull, 0x00, 0x06,        //  1: ifnull +6 → BCI 7
        (u1)Bytecodes::_bipush, 20,                 //  4: bipush 20
        (u1)Bytecodes::_ireturn,                    //  6: ireturn
        (u1)Bytecodes::_bipush, 10,                 //  7: bipush 10
        (u1)Bytecodes::_ireturn,                    //  9: ireturn
    };

    int code_length = sizeof(bytecodes);
    ConstantPool* cp = new ConstantPool(1);
    ConstMethod* cm = new ConstMethod(cp, code_length, 2, 1, 0, 0);
    cm->set_bytecodes(bytecodes, code_length);
    Method* method = new Method(cm, AccessFlags(JVM_ACC_PUBLIC | JVM_ACC_STATIC));

    InstanceKlass* ik = new InstanceKlass();
    ik->set_name("test/NullCheck");
    ik->set_constants(cp);
    ik->set_init_state(InstanceKlass::fully_initialized);

    JavaThread thread("test");
    JavaValue result(T_INT);

    BytecodeInterpreter::_trace_bytecodes = true;
    BytecodeInterpreter::execute(method, ik, &thread, &result);
    BytecodeInterpreter::_trace_bytecodes = false;

    printf("  Result: %d (expected 10)\n", result.get_jint());
    vm_assert(result.get_jint() == 10, "null check should return 10");
    vm_assert(!thread.has_pending_exception(), "no exception");

    delete method;
    delete ik;

    printf("  [PASS] Phase 8 - ifnull OK\n\n");
}

// ---- Test 30: tableswitch ----
// Java 等价代码：
//   int x = 2;
//   switch(x) {
//     case 1: return 10;
//     case 2: return 20;
//     case 3: return 30;
//     default: return -1;
//   }
// 期望结果：20
void test_tableswitch() {
    printf("=== Test: Phase 8 - tableswitch ===\n");

    // tableswitch BCI 布局：
    // BCI  0: iconst_2           → push 2
    // BCI  1: tableswitch        → opcode
    // BCI  2: padding (1 byte to align to 4)
    //         wait... alignment: (bci+4) & ~3 = (1+4) & ~3 = 4
    //         so pad starts at offset 3 from bcp (1 + padding to reach offset 4-1=3)
    //         Actually: aligned = (1 + 4) & ~3 = 4. pad = 4 - 1 = 3.
    //         So bytes at offset 1,2 are padding. Data starts at offset 3.
    //         Actually, pad = aligned - bci = 4 - 1 = 3, meaning offsets 1,2,3 contain:
    //           Actually the code does: int pad = aligned - bci;
    //           and reads from bcp[pad] onwards.
    //           bci=1, aligned=4, pad=3
    //           So data at bcp[3] = offset 3 from opcode
    //           i.e., bytes at positions 2, 3 (after opcode) are padding
    //           byte at position 4 starts default_offset (in the stream, BCI 4)
    //
    // Let me think more carefully:
    //   BCI 0: iconst_2
    //   BCI 1: tableswitch opcode (0xAA)
    //   BCI 2: pad
    //   BCI 3: pad
    //   BCI 4: default_offset bytes [0..3]  (4 bytes)
    //   BCI 8: low [0..3] (4 bytes)
    //   BCI 12: high [0..3] (4 bytes)
    //   BCI 16: offset for case 1 (4 bytes)
    //   BCI 20: offset for case 2 (4 bytes)
    //   BCI 24: offset for case 3 (4 bytes)
    //   BCI 28: (first instruction after switch)
    //
    // Jump targets (offsets from BCI 1):
    //   case 1 → bipush 10 at BCI 28 → offset = 27
    //   case 2 → bipush 20 at BCI 31 → offset = 30
    //   case 3 → bipush 30 at BCI 34 → offset = 33
    //   default → bipush -1 at BCI 37 → wait, let's use bipush for -1
    //     Actually bipush can push -1: bipush 0xFF → signed = -1
    //     Or use iconst_m1
    //
    // Let me layout:
    //   BCI 28: bipush 10, ireturn   (3 bytes: 28,29,30)
    //   BCI 31: bipush 20, ireturn   (3 bytes: 31,32,33)
    //   BCI 34: bipush 30, ireturn   (3 bytes: 34,35,36)
    //   BCI 37: iconst_m1, ireturn   (2 bytes: 37,38)
    //
    // Offsets from BCI 1 (tableswitch opcode):
    //   case 1 → 28 - 1 = 27
    //   case 2 → 31 - 1 = 30
    //   case 3 → 34 - 1 = 33
    //   default → 37 - 1 = 36

    u1 bytecodes[] = {
        (u1)Bytecodes::_iconst_2,       //  0: iconst_2
        (u1)Bytecodes::_tableswitch,    //  1: tableswitch
        0x00, 0x00,                     //  2-3: padding (align to 4)
        // default offset (4 bytes, big-endian): 36
        0x00, 0x00, 0x00, 36,          //  4-7: default = 36 (→ BCI 37)
        // low (4 bytes): 1
        0x00, 0x00, 0x00, 0x01,        //  8-11: low = 1
        // high (4 bytes): 3
        0x00, 0x00, 0x00, 0x03,        // 12-15: high = 3
        // jump offsets for cases 1,2,3
        0x00, 0x00, 0x00, 27,          // 16-19: case 1 → offset 27 (BCI 28)
        0x00, 0x00, 0x00, 30,          // 20-23: case 2 → offset 30 (BCI 31)
        0x00, 0x00, 0x00, 33,          // 24-27: case 3 → offset 33 (BCI 34)
        // case 1 target
        (u1)Bytecodes::_bipush, 10,    // 28-29: bipush 10
        (u1)Bytecodes::_ireturn,       // 30: ireturn
        // case 2 target
        (u1)Bytecodes::_bipush, 20,    // 31-32: bipush 20
        (u1)Bytecodes::_ireturn,       // 33: ireturn
        // case 3 target
        (u1)Bytecodes::_bipush, 30,    // 34-35: bipush 30
        (u1)Bytecodes::_ireturn,       // 36: ireturn
        // default target
        (u1)Bytecodes::_iconst_m1,     // 37: iconst_m1
        (u1)Bytecodes::_ireturn,       // 38: ireturn
    };

    int code_length = sizeof(bytecodes);
    ConstantPool* cp = new ConstantPool(1);
    ConstMethod* cm = new ConstMethod(cp, code_length, 2, 1, 0, 0);
    cm->set_bytecodes(bytecodes, code_length);
    Method* method = new Method(cm, AccessFlags(JVM_ACC_PUBLIC | JVM_ACC_STATIC));

    InstanceKlass* ik = new InstanceKlass();
    ik->set_name("test/Switch");
    ik->set_constants(cp);
    ik->set_init_state(InstanceKlass::fully_initialized);

    JavaThread thread("test");
    JavaValue result(T_INT);

    BytecodeInterpreter::_trace_bytecodes = true;
    BytecodeInterpreter::execute(method, ik, &thread, &result);
    BytecodeInterpreter::_trace_bytecodes = false;

    printf("  Result: %d (expected 20)\n", result.get_jint());
    vm_assert(result.get_jint() == 20, "tableswitch case 2 should return 20");
    vm_assert(!thread.has_pending_exception(), "no exception");

    delete method;
    delete ik;

    printf("  [PASS] Phase 8 - tableswitch OK\n\n");
}

// ---- Test 31: lookupswitch ----
// Java 等价代码：
//   int x = 100;
//   switch(x) {
//     case 10: return 1;
//     case 100: return 2;
//     case 1000: return 3;
//     default: return -1;
//   }
// 期望结果：2
void test_lookupswitch() {
    printf("=== Test: Phase 8 - lookupswitch ===\n");

    // BCI 布局：
    // BCI  0: bipush 100          → push 100
    // BCI  2: lookupswitch        → opcode
    // BCI  3: padding (1 byte to align to 4)
    //         bci=2, aligned=(2+4)&~3=4, pad=4-2=2
    //         But wait: pad includes offset from opcode position.
    //         Actually I need to be more careful.
    //         bci of lookupswitch = 2
    //         aligned = (2 + 4) & ~3 = 4
    //         pad = aligned - bci = 4 - 2 = 2
    //         So data starts at bcp[2], meaning BCI 4
    //
    //   BCI 4: default_offset (4 bytes)
    //   BCI 8: npairs (4 bytes) = 3
    //   BCI 12: match=10, offset (8 bytes)
    //   BCI 20: match=100, offset (8 bytes)
    //   BCI 28: match=1000, offset (8 bytes)
    //   BCI 36: (first instruction after switch)
    //
    // Target offsets (from BCI 2):
    //   case 10  → BCI 36 → offset = 34
    //   case 100 → BCI 39 → offset = 37
    //   case 1000 → BCI 42 → offset = 40
    //   default → BCI 45 → offset = 43

    u1 bytecodes[] = {
        (u1)Bytecodes::_bipush, 100,    //  0-1: bipush 100
        (u1)Bytecodes::_lookupswitch,   //  2: lookupswitch
        0x00,                           //  3: padding (1 byte)
        // default offset (4 bytes): 43
        0x00, 0x00, 0x00, 43,          //  4-7: default = 43 (→ BCI 45)
        // npairs (4 bytes): 3
        0x00, 0x00, 0x00, 0x03,        //  8-11: npairs = 3
        // pair 1: match=10, offset=34
        0x00, 0x00, 0x00, 10,          // 12-15: match = 10
        0x00, 0x00, 0x00, 34,          // 16-19: offset = 34 (→ BCI 36)
        // pair 2: match=100, offset=37
        0x00, 0x00, 0x00, 100,         // 20-23: match = 100
        0x00, 0x00, 0x00, 37,          // 24-27: offset = 37 (→ BCI 39)
        // pair 3: match=1000, offset=40
        0x00, 0x00, 0x03, 0xE8,        // 28-31: match = 1000
        0x00, 0x00, 0x00, 40,          // 32-35: offset = 40 (→ BCI 42)
        // case 10 target
        (u1)Bytecodes::_iconst_1,      // 36: iconst_1
        (u1)Bytecodes::_ireturn,       // 37: ireturn
        (u1)Bytecodes::_nop,           // 38: nop (align)
        // case 100 target
        (u1)Bytecodes::_iconst_2,      // 39: iconst_2
        (u1)Bytecodes::_ireturn,       // 40: ireturn
        (u1)Bytecodes::_nop,           // 41: nop (align)
        // case 1000 target
        (u1)Bytecodes::_iconst_3,      // 42: iconst_3
        (u1)Bytecodes::_ireturn,       // 43: ireturn
        (u1)Bytecodes::_nop,           // 44: nop (align)
        // default target
        (u1)Bytecodes::_iconst_m1,     // 45: iconst_m1
        (u1)Bytecodes::_ireturn,       // 46: ireturn
    };

    int code_length = sizeof(bytecodes);
    ConstantPool* cp = new ConstantPool(1);
    ConstMethod* cm = new ConstMethod(cp, code_length, 2, 1, 0, 0);
    cm->set_bytecodes(bytecodes, code_length);
    Method* method = new Method(cm, AccessFlags(JVM_ACC_PUBLIC | JVM_ACC_STATIC));

    InstanceKlass* ik = new InstanceKlass();
    ik->set_name("test/LookupSwitch");
    ik->set_constants(cp);
    ik->set_init_state(InstanceKlass::fully_initialized);

    JavaThread thread("test");
    JavaValue result(T_INT);

    BytecodeInterpreter::_trace_bytecodes = true;
    BytecodeInterpreter::execute(method, ik, &thread, &result);
    BytecodeInterpreter::_trace_bytecodes = false;

    printf("  Result: %d (expected 2)\n", result.get_jint());
    vm_assert(result.get_jint() == 2, "lookupswitch case 100 should return 2");
    vm_assert(!thread.has_pending_exception(), "no exception");

    delete method;
    delete ik;

    printf("  [PASS] Phase 8 - lookupswitch OK\n\n");
}

// ---- Test 32: long 运算 ----
// Java 等价代码：
//   long a = 1000000000L;  // 10^9
//   long b = 2000000000L;  // 2*10^9
//   long c = a + b;        // 3*10^9 (超出 int 范围)
//   return (int)(c / 1000000000L);  // = 3
// 期望结果：3
void test_long_arithmetic() {
    printf("=== Test: Phase 8 - long arithmetic ===\n");

    // 使用 ldc2_w 来加载 long 常量：需要常量池支持
    // 简化方案：用 i2l 和 imul 构造 long 值
    //
    // 构造 1000000000L:
    //   bipush 100, i2l,           → 100L
    //   bipush 100, i2l, lmul,    → 10000L
    //   bipush 100, i2l, lmul,    → 1000000L
    //   sipush 1000, i2l, lmul,   → 1000000000L
    //   lstore_0
    //
    // 太长了，换个简单方案。
    //
    // 用 lconst_1 和 ladd 做 long 加法，验证基本功能：
    //   lconst_1    → push 1L
    //   lconst_1    → push 1L
    //   ladd        → push 2L
    //   lconst_1    → push 1L
    //   ladd        → push 3L
    //   l2i         → push 3
    //   ireturn     → return 3

    u1 bytecodes[] = {
        (u1)Bytecodes::_lconst_1,      //  0: lconst_1 → 1L
        (u1)Bytecodes::_lconst_1,      //  1: lconst_1 → 1L
        (u1)Bytecodes::_ladd,          //  2: ladd → 2L
        (u1)Bytecodes::_lconst_1,      //  3: lconst_1 → 1L
        (u1)Bytecodes::_ladd,          //  4: ladd → 3L
        (u1)Bytecodes::_l2i,           //  5: l2i → 3
        (u1)Bytecodes::_ireturn,       //  6: ireturn → 3
    };

    int code_length = sizeof(bytecodes);
    ConstantPool* cp = new ConstantPool(1);
    ConstMethod* cm = new ConstMethod(cp, code_length, 6, 1, 0, 0);
    cm->set_bytecodes(bytecodes, code_length);
    Method* method = new Method(cm, AccessFlags(JVM_ACC_PUBLIC | JVM_ACC_STATIC));

    InstanceKlass* ik = new InstanceKlass();
    ik->set_name("test/LongArith");
    ik->set_constants(cp);
    ik->set_init_state(InstanceKlass::fully_initialized);

    JavaThread thread("test");
    JavaValue result(T_INT);

    BytecodeInterpreter::_trace_bytecodes = true;
    BytecodeInterpreter::execute(method, ik, &thread, &result);
    BytecodeInterpreter::_trace_bytecodes = false;

    printf("  Result: %d (expected 3)\n", result.get_jint());
    vm_assert(result.get_jint() == 3, "1L+1L+1L should be 3");
    vm_assert(!thread.has_pending_exception(), "no exception");

    delete method;
    delete ik;

    printf("  [PASS] Phase 8 - long arithmetic OK\n\n");
}

// ---- Test 33: long 存储/加载 + lmul + lcmp ----
// Java 等价代码：
//   long a = 1L;
//   long b = 1L;
//   // 连续乘以 10, 10 次 → a = 10^10 (超过 int 范围)
//   // 简化：a=5L, b=3L, c = a * b = 15L, l2i → 15
//   long a = 5;
//   long b = 3;
//   long c = a * b;    // 15L
//   if (c > 10L) return 1; else return 0;
// 期望结果：1 (因为 15 > 10)
void test_long_store_load_cmp() {
    printf("=== Test: Phase 8 - lstore/lload + lmul + lcmp ===\n");

    // 字节码：
    //  0: iconst_5, i2l        → 5L
    //  2: lstore_0             → local[0,1] = 5L
    //  3: iconst_3, i2l        → 3L
    //  5: lstore_2             → local[2,3] = 3L
    //  6: lload_0              → 5L
    //  7: lload_2              → 3L
    //  8: lmul                 → 15L
    //  9: bipush 10, i2l       → 10L
    // 12: lcmp                 → 1 (15 > 10)
    // 13: ifgt +5              → if (result > 0) goto 18
    // 16: iconst_0
    // 17: ireturn
    // 18: iconst_1
    // 19: ireturn

    u1 bytecodes[] = {
        (u1)Bytecodes::_iconst_5,      //  0: iconst_5
        (u1)Bytecodes::_i2l,           //  1: i2l → 5L
        (u1)Bytecodes::_lstore_0,      //  2: lstore_0
        (u1)Bytecodes::_iconst_3,      //  3: iconst_3
        (u1)Bytecodes::_i2l,           //  4: i2l → 3L
        (u1)Bytecodes::_lstore_2,      //  5: lstore_2
        (u1)Bytecodes::_lload_0,       //  6: lload_0 → 5L
        (u1)Bytecodes::_lload_2,       //  7: lload_2 → 3L
        (u1)Bytecodes::_lmul,          //  8: lmul → 15L
        (u1)Bytecodes::_bipush, 10,    //  9: bipush 10
        (u1)Bytecodes::_i2l,           // 11: i2l → 10L
        (u1)Bytecodes::_lcmp,          // 12: lcmp → 1 (15 > 10)
        (u1)Bytecodes::_ifgt, 0x00, 0x05, // 13: ifgt +5 → BCI 18
        (u1)Bytecodes::_iconst_0,      // 16: iconst_0
        (u1)Bytecodes::_ireturn,       // 17: ireturn
        (u1)Bytecodes::_iconst_1,      // 18: iconst_1
        (u1)Bytecodes::_ireturn,       // 19: ireturn
    };

    int code_length = sizeof(bytecodes);
    ConstantPool* cp = new ConstantPool(1);
    // max_stack=6 (enough for 2 longs + temp), max_locals=4 (2 longs)
    ConstMethod* cm = new ConstMethod(cp, code_length, 6, 4, 0, 0);
    cm->set_bytecodes(bytecodes, code_length);
    Method* method = new Method(cm, AccessFlags(JVM_ACC_PUBLIC | JVM_ACC_STATIC));

    InstanceKlass* ik = new InstanceKlass();
    ik->set_name("test/LongCmp");
    ik->set_constants(cp);
    ik->set_init_state(InstanceKlass::fully_initialized);

    JavaThread thread("test");
    JavaValue result(T_INT);

    BytecodeInterpreter::_trace_bytecodes = true;
    BytecodeInterpreter::execute(method, ik, &thread, &result);
    BytecodeInterpreter::_trace_bytecodes = false;

    printf("  Result: %d (expected 1)\n", result.get_jint());
    vm_assert(result.get_jint() == 1, "5L*3L=15L > 10L should return 1");
    vm_assert(!thread.has_pending_exception(), "no exception");

    delete method;
    delete ik;

    printf("  [PASS] Phase 8 - lstore/lload + lmul + lcmp OK\n\n");
}

// ---- Test 34: pop2 ----
// Java 等价代码：
//   long x = 42L; // push and pop2 to discard
//   return 99;
// 期望结果：99
void test_pop2() {
    printf("=== Test: Phase 8 - pop2 ===\n");

    // 字节码：
    //  0: lconst_1       → push 1L (2 slots)
    //  1: pop2           → discard 2 slots
    //  2: bipush 99      → push 99
    //  4: ireturn        → return 99

    u1 bytecodes[] = {
        (u1)Bytecodes::_lconst_1,      //  0: lconst_1
        (u1)Bytecodes::_pop2,          //  1: pop2
        (u1)Bytecodes::_bipush, 99,    //  2: bipush 99
        (u1)Bytecodes::_ireturn,       //  4: ireturn
    };

    int code_length = sizeof(bytecodes);
    ConstantPool* cp = new ConstantPool(1);
    ConstMethod* cm = new ConstMethod(cp, code_length, 4, 1, 0, 0);
    cm->set_bytecodes(bytecodes, code_length);
    Method* method = new Method(cm, AccessFlags(JVM_ACC_PUBLIC | JVM_ACC_STATIC));

    InstanceKlass* ik = new InstanceKlass();
    ik->set_name("test/Pop2");
    ik->set_constants(cp);
    ik->set_init_state(InstanceKlass::fully_initialized);

    JavaThread thread("test");
    JavaValue result(T_INT);

    BytecodeInterpreter::_trace_bytecodes = true;
    BytecodeInterpreter::execute(method, ik, &thread, &result);
    BytecodeInterpreter::_trace_bytecodes = false;

    printf("  Result: %d (expected 99)\n", result.get_jint());
    vm_assert(result.get_jint() == 99, "pop2 should discard long, return 99");
    vm_assert(!thread.has_pending_exception(), "no exception");

    delete method;
    delete ik;

    printf("  [PASS] Phase 8 - pop2 OK\n\n");
}

// ---- Test 35: tableswitch default 分支 ----
// Java 等价代码：
//   int x = 99;
//   switch(x) {
//     case 1: return 10;
//     case 2: return 20;
//     default: return -1;
//   }
// 期望结果：-1
void test_tableswitch_default() {
    printf("=== Test: Phase 8 - tableswitch (default branch) ===\n");

    // BCI 布局：
    // BCI  0: bipush 99
    // BCI  2: tableswitch
    //   bci=2, aligned=(2+4)&~3=4, pad=2
    //   BCI 3: padding (1 byte)
    //   BCI 4: default_offset (4 bytes)
    //   BCI 8: low = 1 (4 bytes)
    //   BCI 12: high = 2 (4 bytes)
    //   BCI 16: case 1 offset (4 bytes)
    //   BCI 20: case 2 offset (4 bytes)
    //   BCI 24: (first target)
    //
    // Targets (offsets from BCI 2):
    //   case 1 → BCI 24 → offset 22
    //   case 2 → BCI 27 → offset 25
    //   default → BCI 30 → offset 28

    u1 bytecodes[] = {
        (u1)Bytecodes::_bipush, 99,     //  0-1: bipush 99
        (u1)Bytecodes::_tableswitch,    //  2: tableswitch
        0x00,                           //  3: padding
        // default offset: 28
        0x00, 0x00, 0x00, 28,          //  4-7: default = 28 (→ BCI 30)
        // low: 1
        0x00, 0x00, 0x00, 0x01,        //  8-11: low = 1
        // high: 2
        0x00, 0x00, 0x00, 0x02,        // 12-15: high = 2
        // case 1: offset 22
        0x00, 0x00, 0x00, 22,          // 16-19: case 1 → BCI 24
        // case 2: offset 25
        0x00, 0x00, 0x00, 25,          // 20-23: case 2 → BCI 27
        // case 1 target
        (u1)Bytecodes::_bipush, 10,    // 24-25: bipush 10
        (u1)Bytecodes::_ireturn,       // 26: ireturn
        // case 2 target
        (u1)Bytecodes::_bipush, 20,    // 27-28: bipush 20
        (u1)Bytecodes::_ireturn,       // 29: ireturn
        // default target
        (u1)Bytecodes::_iconst_m1,     // 30: iconst_m1
        (u1)Bytecodes::_ireturn,       // 31: ireturn
    };

    int code_length = sizeof(bytecodes);
    ConstantPool* cp = new ConstantPool(1);
    ConstMethod* cm = new ConstMethod(cp, code_length, 2, 1, 0, 0);
    cm->set_bytecodes(bytecodes, code_length);
    Method* method = new Method(cm, AccessFlags(JVM_ACC_PUBLIC | JVM_ACC_STATIC));

    InstanceKlass* ik = new InstanceKlass();
    ik->set_name("test/SwitchDef");
    ik->set_constants(cp);
    ik->set_init_state(InstanceKlass::fully_initialized);

    JavaThread thread("test");
    JavaValue result(T_INT);

    BytecodeInterpreter::_trace_bytecodes = true;
    BytecodeInterpreter::execute(method, ik, &thread, &result);
    BytecodeInterpreter::_trace_bytecodes = false;

    printf("  Result: %d (expected -1)\n", result.get_jint());
    vm_assert(result.get_jint() == -1, "tableswitch default should return -1");
    vm_assert(!thread.has_pending_exception(), "no exception");

    delete method;
    delete ik;

    printf("  [PASS] Phase 8 - tableswitch default OK\n\n");
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    printf("========================================\n");
    printf("  Mini JVM - Phase 8: Long/Switch/Ref\n");
    printf("========================================\n\n");

    // Phase 1 基础测试
    test_bytes();
    test_classfile_stream();

    // Phase 3 测试
    test_mark_word();
    test_oop_header();
    test_access_flags();
    test_metadata_method();
    test_klass();

    // Phase 3 完整流程
    if (argc > 1) {
        test_full_pipeline(argv[1]);
    } else {
        test_full_pipeline("test/HelloWorld.class");
    }

    // Phase 4 测试
    test_bytecodes();
    test_java_thread();
    test_interpreter_frame();
    test_interpreter_simple_add();
    test_interpreter_with_args();
    test_interpreter_branch();

    // Phase 4 终极测试：解析 + 执行 HelloWorld.class
    if (argc > 1) {
        test_full_execution(argv[1]);
    } else {
        test_full_execution("test/HelloWorld.class");
    }

    // Phase 5 测试
    test_java_heap_basic();
    test_object_allocation();
    test_interpreter_object_creation();
    test_interpreter_static_fields();

    // Phase 6 测试
    test_init_constructor();
    test_invokevirtual_instance_method();
    test_multiple_method_calls();
    test_init_with_args();

    // Phase 7 测试
    test_type_array_klass_basic();
    test_newarray_int();
    test_arraylength();
    test_byte_array();
    test_array_loop_sum();

    // Phase 8 新增测试
    test_if_acmpeq();
    test_ifnull();
    test_tableswitch();
    test_lookupswitch();
    test_long_arithmetic();
    test_long_store_load_cmp();
    test_pop2();
    test_tableswitch_default();

    printf("========================================\n");
    printf("  All Phase 8 tests completed!\n");
    printf("========================================\n");

    return 0;
}
