/*
 * my_jvm - Memory allocation base classes
 * 
 * 参考 OpenJDK 11 hotspot/src/hotspot/share/memory/allocation.hpp
 * 简化版本，提供内存分配基类
 */

#ifndef MY_JVM_MEMORY_ALLOCATION_HPP
#define MY_JVM_MEMORY_ALLOCATION_HPP

#include "utilities/globalDefinitions.hpp"
#include "utilities/debug.hpp"
#include <new>
#include <cstdlib>

// ========== 内存类型标志 ==========

enum MemoryType {
  mtJavaHeap,          // Java 堆
  mtClass,             // 类元数据
  mtThread,            // 线程对象
  mtThreadStack,       // 线程栈
  mtCode,              // 生成代码
  mtGC,                // GC 内存
  mtCompiler,          // 编译器内存
  mtInternal,          // VM 内部内存
  mtOther,             // 其他
  mtSymbol,            // 符号表
  mtNMT,               // Native Memory Tracking
  mtClassShared,       // 类数据共享
  mtChunk,             // Arena 块
  mtTest,              // 测试
  mtTracing,           // 追踪
  mtLogging,           // 日志
  mtArguments,         // 参数处理
  mtModule,            // 模块处理
  mtSynchronizer,      // 同步原语
  mtSafepoint,         // Safepoint 支持
  mtNone,              // 未定义
  mt_number_of_types
};

typedef MemoryType MEMFLAGS;

// ========== 分配失败策略 ==========

class AllocFailStrategy {
public:
  enum AllocFailEnum { EXIT_OOM, RETURN_NULL };
};
typedef AllocFailStrategy::AllocFailEnum AllocFailType;

// ========== 堆分配函数 ==========

inline char* AllocateHeap(size_t size, MEMFLAGS flags, 
                         AllocFailType alloc_failmode = AllocFailStrategy::EXIT_OOM) {
  char* p = (char*)std::malloc(size);
  if (p == nullptr && alloc_failmode == AllocFailStrategy::EXIT_OOM) {
    fprintf(stderr, "Out of memory: requested %zu bytes\n", size);
    std::abort();
  }
  return p;
}

inline void FreeHeap(void* p) {
  std::free(p);
}

// ========== CHeapObj - C 堆分配对象基类 ==========

template <MEMFLAGS F> class CHeapObj {
 public:
  void* operator new(size_t size) throw() {
    return (void*)AllocateHeap(size, F);
  }

  void* operator new(size_t size, const std::nothrow_t&) throw() {
    return (void*)AllocateHeap(size, F, AllocFailStrategy::RETURN_NULL);
  }

  void* operator new[](size_t size) throw() {
    return (void*)AllocateHeap(size, F);
  }

  void* operator new[](size_t size, const std::nothrow_t&) throw() {
    return (void*)AllocateHeap(size, F, AllocFailStrategy::RETURN_NULL);
  }

  void operator delete(void* p)     { FreeHeap(p); }
  void operator delete[](void* p)   { FreeHeap(p); }
};

// ========== StackObj - 栈对象基类 ==========

class StackObj {
 private:
  void* operator new(size_t size) throw();
  void* operator new[](size_t size) throw();
  void  operator delete(void* p);
  void  operator delete[](void* p);
};

// ========== ResourceObj - 资源区对象基类 ==========

class ResourceObj {
 public:
  void* operator new(size_t size);
  void  operator delete(void* p);
};

// ========== AllStatic - 纯静态类基类 ==========

class AllStatic {
 private:
  AllStatic();
  ~AllStatic();
};

// ========== 便捷宏 ==========

#define NEW_C_HEAP_ARRAY(type, size, flags) \
  (type*)AllocateHeap((size) * sizeof(type), flags)

#define FREE_C_HEAP_ARRAY(type, array) \
  FreeHeap((void*)array)

#define NEW_C_HEAP_OBJ(type, flags) \
  NEW_C_HEAP_ARRAY(type, 1, flags)

#define FREE_C_HEAP_OBJ(objname, type, flags) \
  FreeHeap((void*)objname)

#endif // MY_JVM_MEMORY_ALLOCATION_HPP
