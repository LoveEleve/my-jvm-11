#ifndef SHARE_MEMORY_ALLOCATION_HPP
#define SHARE_MEMORY_ALLOCATION_HPP

// ============================================================================
// Mini JVM - 内存分配基类体系
// 对应 HotSpot: src/hotspot/share/memory/allocation.hpp
//
// HotSpot 有 5 大分配基类，我们先实现最核心的 3 个：
//   CHeapObj  — C 堆对象 (malloc/free)
//   StackObj  — 栈对象 (禁止 new)
//   AllStatic — 纯静态类 (无实例)
//
// ResourceObj 和 MetaspaceObj 后续按需添加。
// ============================================================================

#include "utilities/globalDefinitions.hpp"
#include "utilities/debug.hpp"

// ----------------------------------------------------------------------------
// MemoryType (MEMFLAGS)
// HotSpot: allocation.hpp 约 115-142 行
// 用于 NMT (Native Memory Tracking) 分类追踪内存用途
// ----------------------------------------------------------------------------

enum MemoryType {
    mtJavaHeap,        // Java 堆
    mtClass,           // 类元数据
    mtThread,          // 线程数据
    mtThreadStack,     // 线程栈
    mtCode,            // 编译后的代码
    mtGC,              // GC 数据
    mtCompiler,        // 编译器
    mtInternal,        // 内部使用
    mtOther,           // 其他
    mtSymbol,          // 符号表
    mtNMT,             // NMT 自身
    mtChunk,           // chunk
    mtTest,            // 测试
    mtNone,            // 无分类
    mt_number_of_types
};

typedef MemoryType MEMFLAGS;

// ----------------------------------------------------------------------------
// 分配失败策略
// HotSpot: allocation.hpp 中 AllocFailStrategy 类
// ----------------------------------------------------------------------------

class AllocFailStrategy {
public:
    enum AllocFailEnum { EXIT_OOM, RETURN_NULL };
};

// ----------------------------------------------------------------------------
// 底层分配/释放函数
// HotSpot 版本会经过 NMT 追踪，我们直接调 malloc/free
// ----------------------------------------------------------------------------

inline void* AllocateHeap(size_t size, MEMFLAGS flags,
                          AllocFailStrategy::AllocFailEnum alloc_failmode =
                              AllocFailStrategy::EXIT_OOM) {
    void* p = ::malloc(size);
    if (p == nullptr && alloc_failmode == AllocFailStrategy::EXIT_OOM) {
        report_vm_error(__FILE__, __LINE__,
            "OutOfMemoryError", "AllocateHeap: malloc failed");
    }
    return p;
}

inline void* ReallocateHeap(void* old_ptr, size_t size, MEMFLAGS flags) {
    void* p = ::realloc(old_ptr, size);
    if (p == nullptr) {
        report_vm_error(__FILE__, __LINE__,
            "OutOfMemoryError", "ReallocateHeap: realloc failed");
    }
    return p;
}

inline void FreeHeap(void* p) {
    ::free(p);
}

// ============================================================================
// CHeapObj — C 堆对象基类
// HotSpot: allocation.hpp 约 175-215 行
//
// 通过 malloc 分配，通过 free 释放。
// 模板参数 F 是 MEMFLAGS，标识这块内存的用途（用于 NMT 追踪）。
// HotSpot 中几乎所有需要 new 出来的 VM 内部对象都继承自这个类。
// ============================================================================

template <MEMFLAGS F>
class CHeapObj {
public:
    void* operator new(size_t size) throw() {
        return AllocateHeap(size, F);
    }

    void* operator new(size_t size,
                       AllocFailStrategy::AllocFailEnum alloc_failmode) throw() {
        return AllocateHeap(size, F, alloc_failmode);
    }

    void operator delete(void* p) {
        FreeHeap(p);
    }

    // 数组版本
    void* operator new[](size_t size) throw() {
        return AllocateHeap(size, F);
    }

    void operator delete[](void* p) {
        FreeHeap(p);
    }
};

// ============================================================================
// StackObj — 栈对象基类
// HotSpot: allocation.hpp 约 322-332 行
//
// 只允许在栈上创建，禁止 new 和 delete。
// 典型子类：HandleMark, ResourceMark, MutexLocker 等 RAII 类。
// ============================================================================

class StackObj {
private:
    // 禁止在堆上分配
    void* operator new(size_t size) throw();
    void* operator new[](size_t size) throw();
    void  operator delete(void* p);
    void  operator delete[](void* p);
};

// ============================================================================
// AllStatic — 纯静态类（命名空间替代品）
// HotSpot: allocation.hpp 约 335-350 行
//
// 不允许创建任何实例，所有成员都是 static 的。
// 典型子类：Universe, SystemDictionary, os 等。
// ============================================================================

class AllStatic {
public:
    AllStatic()  { ShouldNotCallThis(); }
    ~AllStatic() { ShouldNotCallThis(); }
};

// ============================================================================
// 分配宏（便捷接口）
// HotSpot: allocation.hpp 尾部
// ============================================================================

// C 堆分配数组
#define NEW_C_HEAP_ARRAY(type, size, memflags) \
    ((type*)AllocateHeap((size) * sizeof(type), memflags))

// C 堆分配单个对象
#define NEW_C_HEAP_OBJ(type, memflags) \
    NEW_C_HEAP_ARRAY(type, 1, memflags)

// C 堆释放
#define FREE_C_HEAP_ARRAY(type, old) \
    FreeHeap((char*)(old))

#define FREE_C_HEAP_OBJ(obj) \
    FreeHeap((char*)(obj))

#endif // SHARE_MEMORY_ALLOCATION_HPP
