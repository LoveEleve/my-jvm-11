#ifndef SHARE_RUNTIME_VM_HPP
#define SHARE_RUNTIME_VM_HPP

// ============================================================================
// Mini JVM - VM（虚拟机生命周期管理）
// 对照 HotSpot: Threads::create_vm() [thread.cpp:3876]
//             + Threads::destroy_vm() [thread.cpp:4313]
//
// VM 类统一管理虚拟机的启动和关闭。
//
// 启动流程（对照 Threads::create_vm）:
//   1. vm_init_globals()  — 基础设施初始化
//   2. 创建主线程 JavaThread
//   3. init_globals()     — 核心模块初始化
//      ├─ bytecodes_init()
//      ├─ ClassLoader::initialize()
//      ├─ Universe::initialize()      — 堆创建
//      ├─ Universe::genesis()         — 基本类型 Klass
//      ├─ SystemDictionary::initialize()
//      └─ Universe::post_initialize()
//   4. VM 就绪
// ============================================================================

#include "utilities/globalDefinitions.hpp"

class JavaThread;

class VM {
public:
    // 创建 VM
    // 对照: Threads::create_vm() [thread.cpp:3876]
    static bool create_vm();

    // 销毁 VM
    // 对照: Threads::destroy_vm() [thread.cpp:4313]
    static void destroy_vm();

    // 全局状态
    static bool is_initialized() { return _initialized; }
    static JavaThread* main_thread() { return _main_thread; }

private:
    static bool _initialized;
    static JavaThread* _main_thread;

    // 对照: vm_init_globals() [init.cpp:90]
    static void vm_init_globals();

    // 对照: init_globals() [init.cpp:104]
    static jint init_globals();
};

#endif // SHARE_RUNTIME_VM_HPP
