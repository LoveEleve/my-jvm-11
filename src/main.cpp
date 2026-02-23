// ============================================================================
// Mini JVM — 程序入口
// 对照 HotSpot: src/java.base/share/native/launcher/main.c
//             + src/java.base/share/native/libjli/java.c (JavaMain)
//
// 执行链:
//   main() → Arguments::parse() → VM::create_vm()
//          → LoadMainClass → find main() → JavaCalls::call_static()
//          → VM::destroy_vm()
//
// 对照 HotSpot 真实执行链:
//   main.c:97 → JLI_Launch() → JVMInit() → ContinueInNewThread()
//   → JavaMain() [java.c:486]
//     → InitializeJVM()         — JNI_CreateJavaVM → Threads::create_vm()
//     → LoadMainClass()         — Class.forName(mainclass)
//     → GetStaticMethodID()     — 查找 main 方法
//     → CallStaticVoidMethod()  — 执行 main 方法
//     → LEAVE()                 — DestroyJavaVM
// ============================================================================

#include "runtime/arguments.hpp"
#include "runtime/vm.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/javaThread.hpp"
#include "classfile/systemDictionary.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/method.hpp"

#include <cstdio>
#include <cstdlib>

// 外部声明：回归测试入口（定义在 test/main.cpp 中）
extern int run_regression_tests(int argc, char** argv);

int main(int argc, char** argv) {
    // ═══════════════════════════════════════════════════════════════
    // 对照: JavaMain() [java.c:486-656]
    // ═══════════════════════════════════════════════════════════════

    // ═══ Step 1: 解析参数 ═══
    // 对照: ParseArguments() [java.c:414]
    if (!Arguments::parse(argc, argv)) {
        Arguments::print_usage();
        return 1;
    }

    // ═══ 测试模式：运行回归测试 ═══
    if (Arguments::is_test_mode()) {
        return run_regression_tests(argc, argv);
    }

    // ═══ Step 2: 创建 VM ═══
    // 对照: InitializeJVM() [java.c:1603]
    //   → ifn->CreateJavaVM() → JNI_CreateJavaVM() → Threads::create_vm()
    if (!VM::create_vm()) {
        fprintf(stderr, "Error: Could not create the Java Virtual Machine.\n");
        return 1;
    }

    JavaThread* thread = VM::main_thread();

    // ═══ Step 3: 加载主类 ═══
    // 对照: LoadMainClass() [java.c:604]
    //   HotSpot: LauncherHelper.checkAndLoadMain()
    //          → Class.forName() → SystemDictionary::resolve_or_null()
    const char* main_class_name = Arguments::main_class_name();
    InstanceKlass* main_klass = SystemDictionary::resolve_or_null(main_class_name, thread);

    if (main_klass == nullptr) {
        fprintf(stderr, "Error: Could not find or load main class %s\n",
                main_class_name);
        VM::destroy_vm();
        return 1;
    }

    // ═══ Step 4: 初始化主类（执行 <clinit>）═══
    // 对照: 在 JNI 调用路径中，ensure_initialized() 会自动触发
    //       InstanceKlass::initialize_impl() [instanceKlass.cpp:892]
    if (!main_klass->is_initialized()) {
        // 查找 <clinit>
        Method* clinit = main_klass->find_method("<clinit>", "()V");
        if (clinit != nullptr) {
            fprintf(stderr, "[VM] Executing <clinit> for %s\n", main_class_name);
            JavaValue clinit_result(T_VOID);
            JavaCalls::call_static(&clinit_result, main_klass, clinit, thread);
        }
        main_klass->set_init_state(InstanceKlass::fully_initialized);
    }

    // ═══ Step 5: 查找 main 方法 ═══
    // 对照: GetStaticMethodID(env, mainClass, "main", "([Ljava/lang/String;)V")
    // [java.c:641]
    //
    // 注意: 我们同时尝试两种签名:
    //   "([Ljava/lang/String;)V"  — 标准签名
    //   "()V"                     — 无参 main（测试用简化版）
    Method* main_method = main_klass->find_method("main", "([Ljava/lang/String;)V");
    if (main_method == nullptr) {
        // 尝试无参 main（为了支持简单测试类）
        main_method = main_klass->find_method("main", "()V");
    }

    if (main_method == nullptr) {
        fprintf(stderr, "Error: Main method not found in class %s, please define as:\n"
                "  public static void main(String[] args)\n", main_class_name);
        VM::destroy_vm();
        return 1;
    }

    fprintf(stderr, "[VM] Found main method: %s%s\n",
            main_method->name(), main_method->signature());

    // ═══ Step 6: 调用 main 方法 ═══
    // 对照: (*env)->CallStaticVoidMethod(env, mainClass, mainID, mainArgs)
    // [java.c:647]
    //   → jni_CallStaticVoidMethod() [jni.cpp:1984]
    //     → jni_invoke_static() [jni.cpp:1108]
    //       → JavaCalls::call() [javaCalls.cpp:339]
    //         → call_helper() [javaCalls.cpp:348]
    //           → StubRoutines::call_stub()() → 解释器
    fprintf(stderr, "[VM] Calling %s.main()\n", main_class_name);
    fprintf(stderr, "----------------------------------------\n");

    JavaValue result(T_VOID);
    JavaCalls::call_static(&result, main_klass, main_method, thread);

    fprintf(stderr, "----------------------------------------\n");

    // 检查异常
    if (thread->has_pending_exception()) {
        fprintf(stderr, "Exception in thread \"main\": %s\n",
                thread->exception_message() ? thread->exception_message() : "(unknown)");
    }

    // ═══ Step 7: 销毁 VM ═══
    // 对照: LEAVE() → DetachCurrentThread() → DestroyJavaVM()
    // [java.c:651]
    VM::destroy_vm();

    return 0;
}
