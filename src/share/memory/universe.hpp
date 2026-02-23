#ifndef SHARE_MEMORY_UNIVERSE_HPP
#define SHARE_MEMORY_UNIVERSE_HPP

// ============================================================================
// Mini JVM - Universe（JVM 全局状态）
// 对照 HotSpot: src/hotspot/share/memory/universe.hpp
//
// Universe 是 JVM 的 "宇宙" — 包含所有全局状态。
// HotSpot 中 Universe 是纯静态类（全部 static 成员）。
//
// 核心职责：
//   1. 管理 Java 堆 (_collectedHeap / 我们的 JavaHeap)
//   2. 持有 8 种基本类型的 TypeArrayKlass
//   3. 持有预分配的异常对象（OOM/NPE 等）
//   4. 管理 VM 的初始化阶段
//
// 初始化顺序（对照 init.cpp）：
//   universe_init()      → 创建堆 + SymbolTable + StringTable
//   universe2_init()     → genesis() → 创建基本类型 Klass
//   universe_post_init() → 预分配异常对象
// ============================================================================

#include "utilities/globalDefinitions.hpp"

class JavaHeap;
class TypeArrayKlass;

class Universe {
public:
    // ══════ 初始化入口 ══════

    // 对照 universe_init() [universe.cpp:681]
    // 创建堆
    static jint initialize();

    // 对照 universe2_init() → genesis() [universe.cpp:1200]
    // 创建基本类型 TypeArrayKlass
    static void genesis();

    // 对照 universe_post_init() [universe.cpp:1210]
    // 预分配异常对象（当前为空壳，Phase 15 完善）
    static bool post_initialize();

    // ══════ 全局访问 ══════

    static JavaHeap* heap() { return _heap; }

    // 通过 BasicType 获取对应的 TypeArrayKlass
    // 对照 HotSpot: Universe::typeArrayKlassObj(BasicType t)
    static TypeArrayKlass* typeArrayKlass(BasicType t);

    // 便捷访问
    static TypeArrayKlass* boolArrayKlass()   { return typeArrayKlass(T_BOOLEAN); }
    static TypeArrayKlass* byteArrayKlass()   { return typeArrayKlass(T_BYTE); }
    static TypeArrayKlass* charArrayKlass()   { return typeArrayKlass(T_CHAR); }
    static TypeArrayKlass* shortArrayKlass()  { return typeArrayKlass(T_SHORT); }
    static TypeArrayKlass* intArrayKlass()    { return typeArrayKlass(T_INT); }
    static TypeArrayKlass* longArrayKlass()   { return typeArrayKlass(T_LONG); }
    static TypeArrayKlass* floatArrayKlass()  { return typeArrayKlass(T_FLOAT); }
    static TypeArrayKlass* doubleArrayKlass() { return typeArrayKlass(T_DOUBLE); }

    static bool is_fully_initialized() { return _fully_initialized; }

    // ══════ 清理 ══════
    static void destroy();

private:
    static JavaHeap*       _heap;
    static TypeArrayKlass* _typeArrayKlassObjs[T_CONFLICT + 1];
    static bool            _fully_initialized;
};

#endif // SHARE_MEMORY_UNIVERSE_HPP
