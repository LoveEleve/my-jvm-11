#include "runtime/vm.hpp"
#include "runtime/arguments.hpp"
#include "runtime/javaThread.hpp"
#include "memory/universe.hpp"
#include "classfile/classLoader.hpp"
#include "classfile/systemDictionary.hpp"

#include <cstdio>

// ============================================================================
// VM — 虚拟机生命周期管理
// 对照 HotSpot: Threads::create_vm() [thread.cpp:3876]
// ============================================================================

bool        VM::_initialized = false;
JavaThread* VM::_main_thread = nullptr;

bool VM::create_vm() {
    // ═══════════════════════════════════════════════════════════════
    // 对照: Threads::create_vm() [thread.cpp:3876-4306]
    //
    // HotSpot 完整流程（50 个步骤，我们实现关键步骤）:
    //
    //   1.  VM_Version::early_initialize()          → 跳过
    //   2.  ThreadLocalStorage::init()              → 跳过（单线程）
    //   3.  ostream_init()                          → 跳过
    //   4.  os::init()                              → 跳过
    //   5.  Arguments::parse()                      → 已在 main() 中完成
    //   6-11. OS/SafePoint 初始化                    → 跳过
    //   12. vm_init_globals()                       → ★ 实现
    //   13. new JavaThread()                        → ★ 实现
    //   14. init_globals()                          → ★ 实现
    //   15-50. VMThread/JIT/服务线程等              → 跳过
    // ═══════════════════════════════════════════════════════════════

    fprintf(stderr, "========================================\n");
    fprintf(stderr, "  Mini JVM Starting...\n");
    fprintf(stderr, "========================================\n");

    // ═══ 阶段 1: vm_init_globals() ═══
    // 对照: thread.cpp:4002
    vm_init_globals();

    // ═══ 阶段 2: 创建主线程 ═══
    // 对照: thread.cpp:4018-4055
    //   new JavaThread()
    //   set_thread_state(_thread_in_vm)
    //   initialize_thread_current()          → TLS 绑定（我们用全局指针）
    //   record_stack_base_and_size()         → 跳过
    //   set_as_starting_thread()             → 跳过
    //   create_stack_guard_pages()           → 跳过
    _main_thread = new JavaThread("main");
    _main_thread->set_thread_state(_thread_in_vm);

    fprintf(stderr, "[VM] Main thread created: %p\n", (void*)_main_thread);

    // ═══ 阶段 3: init_globals() ═══
    // 对照: thread.cpp:4060
    jint status = init_globals();
    if (status != 0) {
        fprintf(stderr, "[VM] Error: init_globals() failed\n");
        return false;
    }

    // ═══ 完成 ═══
    // 对照: thread.cpp:4139 set_init_completed()
    _initialized = true;

    fprintf(stderr, "[VM] VM created successfully\n");
    fprintf(stderr, "========================================\n");

    return true;
}

void VM::vm_init_globals() {
    // 对照: vm_init_globals() [init.cpp:90-98]
    //
    // HotSpot:
    //   check_ThreadShadow()       → 跳过
    //   basic_types_init()         → 验证类型大小
    //   eventlog_init()            → 跳过
    //   mutex_init()               → 跳过（单线程）
    //   chunkpool_init()           → 跳过
    //   perfMemory_init()          → 跳过
    //   SuspendibleThreadSet_init() → 跳过

    // basic_types_init: 验证类型大小
    // 对照: globalDefinitions.cpp basic_types_init()
    vm_assert(sizeof(jbyte)  == 1, "jbyte size check");
    vm_assert(sizeof(jshort) == 2, "jshort size check");
    vm_assert(sizeof(jint)   == 4, "jint size check");
    vm_assert(sizeof(jlong)  == 8, "jlong size check");
    vm_assert(sizeof(jfloat) == 4, "jfloat size check");
    vm_assert(sizeof(jdouble)== 8, "jdouble size check");

    fprintf(stderr, "[VM] vm_init_globals: basic_types verified\n");
}

jint VM::init_globals() {
    // 对照: init_globals() [init.cpp:104-168]
    //
    // HotSpot 完整初始化顺序:
    //   management_init()               → 跳过
    //   bytecodes_init()                → ★ (已有字节码定义)
    //   classLoader_init1()             → ★ ClassLoader::initialize()
    //   compilationPolicy_init()        → 跳过
    //   codeCache_init()                → 跳过
    //   VM_Version_init()               → 跳过
    //   stubRoutines_init1()            → 跳过
    //   universe_init()                 → ★ Universe::initialize()
    //   gc_barrier_stubs_init()         → 跳过
    //   interpreter_init()              → (我们的 switch-case 解释器不需要初始化)
    //   universe2_init()                → ★ Universe::genesis()
    //   javaClasses_init()              → 跳过
    //   universe_post_init()            → ★ Universe::post_initialize()

    // bytecodes_init — 字节码表已在编译时通过枚举定义
    fprintf(stderr, "[VM] init_globals: bytecodes ready\n");

    // ClassLoader 初始化
    ClassLoader::initialize();

    // Universe 初始化 — 创建堆
    jint status = Universe::initialize();
    if (status != 0) {
        return status;
    }

    // Universe genesis — 创建基本类型 TypeArrayKlass
    Universe::genesis();

    // SystemDictionary 初始化
    SystemDictionary::initialize();

    // Universe post_initialize — 预分配异常对象
    Universe::post_initialize();

    fprintf(stderr, "[VM] init_globals: all modules initialized\n");

    return 0;  // JNI_OK
}

void VM::destroy_vm() {
    // 对照: Threads::destroy_vm() [thread.cpp:4313]
    //
    // HotSpot:
    //   wait for non-daemon threads
    //   VMThread::wait_for_vm_thread_exit()
    //   JvmtiExport::post_vm_death()
    //   Universe::heap()->stop()
    //   ...

    fprintf(stderr, "========================================\n");
    fprintf(stderr, "  Mini JVM Shutting down...\n");
    fprintf(stderr, "========================================\n");

    // 销毁 SystemDictionary
    SystemDictionary::destroy();

    // 销毁 Universe（堆 + TypeArrayKlass）
    Universe::destroy();

    // 销毁主线程
    if (_main_thread != nullptr) {
        delete _main_thread;
        _main_thread = nullptr;
    }

    _initialized = false;

    fprintf(stderr, "[VM] Shutdown complete\n");
}
