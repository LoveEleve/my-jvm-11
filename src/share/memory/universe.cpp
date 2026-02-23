#include "memory/universe.hpp"
#include "gc/shared/javaHeap.hpp"
#include "oops/typeArrayKlass.hpp"
#include "runtime/arguments.hpp"

// ============================================================================
// Universe — JVM 全局状态
// 对照 HotSpot: src/hotspot/share/memory/universe.cpp
// ============================================================================

// 静态成员初始化
JavaHeap*       Universe::_heap = nullptr;
TypeArrayKlass* Universe::_typeArrayKlassObjs[T_CONFLICT + 1] = { nullptr };
bool            Universe::_fully_initialized = false;

jint Universe::initialize() {
    // 对照: universe_init() [universe.cpp:681]
    // HotSpot:
    //   JavaClasses::compute_hard_coded_offsets()  → 跳过
    //   Universe::initialize_heap()                → 创建堆
    //   Metaspace::global_initialize()             → 跳过（我们用 malloc）
    //   SymbolTable::create_table()                → 跳过（我们用 char*）
    //   StringTable::create_table()                → 跳过

    // ═══ 创建 Java 堆 ═══
    // 对照: Universe::initialize_heap() [universe.cpp:694]
    // HotSpot: _collectedHeap = create_heap() → GCArguments::create_heap()
    //          → G1CollectedHeap → ReservedSpace → mmap
    //
    // 使用 JavaHeap::initialize() 保持与已有代码兼容
    // JavaHeap 自己维护 _heap 单例，Universe 也持有一个引用
    size_t heap_size = Arguments::heap_size();
    JavaHeap::initialize(heap_size);
    _heap = JavaHeap::heap();

    if (_heap == nullptr) {
        fprintf(stderr, "[VM] Error: Failed to create Java heap (%zu bytes)\n", heap_size);
        return -1;  // JNI_ERR
    }

    fprintf(stderr, "[VM] Universe::initialize: heap created (%zuMB)\n",
            heap_size / M);

    return 0;  // JNI_OK
}

void Universe::genesis() {
    // 对照: universe2_init() → Universe::genesis() [universe.cpp:1079-1194]
    //
    // HotSpot 中创建 8 种基本类型的 TypeArrayKlass:
    //   _typeArrayKlassObjs[T_BOOLEAN] = TypeArrayKlass::create_klass(T_BOOLEAN, CHECK)
    //   ... (bool, char, float, double, byte, short, int, long)
    //
    // 使用 TypeArrayKlass::initialize_all() 保持与已有代码兼容
    // TypeArrayKlass 自己维护 _type_array_klasses[] 静态数组
    TypeArrayKlass::initialize_all();

    // 同步到 Universe 的引用
    _typeArrayKlassObjs[T_BOOLEAN] = TypeArrayKlass::for_type(T_BOOLEAN);
    _typeArrayKlassObjs[T_CHAR]    = TypeArrayKlass::for_type(T_CHAR);
    _typeArrayKlassObjs[T_FLOAT]   = TypeArrayKlass::for_type(T_FLOAT);
    _typeArrayKlassObjs[T_DOUBLE]  = TypeArrayKlass::for_type(T_DOUBLE);
    _typeArrayKlassObjs[T_BYTE]    = TypeArrayKlass::for_type(T_BYTE);
    _typeArrayKlassObjs[T_SHORT]   = TypeArrayKlass::for_type(T_SHORT);
    _typeArrayKlassObjs[T_INT]     = TypeArrayKlass::for_type(T_INT);
    _typeArrayKlassObjs[T_LONG]    = TypeArrayKlass::for_type(T_LONG);

    fprintf(stderr, "[VM] Universe::genesis: 8 TypeArrayKlasses created\n");

    // HotSpot 中接下来还做:
    //   vmSymbols::initialize()
    //   SystemDictionary::initialize()   → 在 VM::init_globals() 中处理
}

bool Universe::post_initialize() {
    // 对照: universe_post_init() [universe.cpp:1210-1320]
    // HotSpot:
    //   _fully_initialized = true
    //   Interpreter::initialize()         → entry points（我们用 switch-case 不需要）
    //   预分配 OutOfMemoryError (6种)
    //   预分配 NullPointerException
    //   预分配 ArithmeticException
    //   heap()->post_initialize()

    _fully_initialized = true;

    // 预分配异常对象 → Phase 15 完善
    // 目前只设置标记

    fprintf(stderr, "[VM] Universe::post_initialize: fully initialized\n");
    return true;
}

TypeArrayKlass* Universe::typeArrayKlass(BasicType t) {
    vm_assert(t >= T_BOOLEAN && t <= T_LONG, "invalid BasicType for typeArrayKlass");
    vm_assert(_typeArrayKlassObjs[t] != nullptr, "typeArrayKlass not initialized");
    return _typeArrayKlassObjs[t];
}

void Universe::destroy() {
    // 销毁 TypeArrayKlass（通过 TypeArrayKlass 自己的方法）
    TypeArrayKlass::destroy_all();
    for (int i = 0; i <= T_CONFLICT; i++) {
        _typeArrayKlassObjs[i] = nullptr;
    }

    // 销毁堆（通过 JavaHeap 自己的方法）
    JavaHeap::destroy();
    _heap = nullptr;

    _fully_initialized = false;
}
