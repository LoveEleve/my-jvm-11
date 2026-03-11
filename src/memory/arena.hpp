/*
 * my_jvm - Arena memory allocation
 * 
 * 参考 OpenJDK 11 hotspot/src/hotspot/share/memory/arena.hpp
 * 简化版本，提供快速内存分配 Arena
 */

#ifndef MY_JVM_MEMORY_ARENA_HPP
#define MY_JVM_MEMORY_ARENA_HPP

#include "memory/allocation.hpp"
#include "utilities/globalDefinitions.hpp"
#include "utilities/debug.hpp"
#include <new>

// ========== 对齐常量 ==========

#define ARENA_AMALLOC_ALIGNMENT (2 * sizeof(void*))
#define ARENA_ALIGN_M1 (((size_t)(ARENA_AMALLOC_ALIGNMENT)) - 1)
#define ARENA_ALIGN_MASK (~((size_t)ARENA_ALIGN_M1))
#define ARENA_ALIGN(x) ((((size_t)(x)) + ARENA_ALIGN_M1) & ARENA_ALIGN_MASK)

// ========== Chunk - 内存块 ==========

class Chunk : public CHeapObj<mtChunk> {
 private:
  Chunk*       _next;     // 链表下一个 Chunk
  const size_t _len;      // 本 Chunk 数据区大小

 public:
  // Chunk 大小常量
  enum {
#ifdef _LP64
    slack = 40,
#else
    slack = 20,
#endif
    tiny_size   = 256  - slack,
    init_size   = 1024 - slack,
    medium_size = 10 * 1024 - slack,
    size        = 32 * 1024 - slack
  };

  Chunk(size_t length) : _next(nullptr), _len(length) {}
  
  void* operator new(size_t size, size_t length) {
    return AllocateHeap(ARENA_ALIGN(sizeof(Chunk)) + length, mtChunk);
  }
  
  void operator delete(void* p) { FreeHeap(p); }

  // 边界
  char* bottom() const { return ((char*)this) + ARENA_ALIGN(sizeof(Chunk)); }
  char* top() const    { return bottom() + _len; }
  bool contains(char* p) const { return bottom() <= p && p <= top(); }

  size_t length() const   { return _len; }
  Chunk* next() const     { return _next; }
  void set_next(Chunk* n) { _next = n; }

  static size_t aligned_overhead_size() { return ARENA_ALIGN(sizeof(Chunk)); }
};

// ========== Arena - 快速内存分配区 ==========

class Arena : public CHeapObj<mtNone> {
 protected:
  MEMFLAGS _flags;         // 内存类型标志
  Chunk*   _first;         // 第一个 Chunk
  Chunk*   _chunk;         // 当前 Chunk
  char*    _hwm;           // 当前 Chunk 高水位（分配指针）
  char*    _max;           // 当前 Chunk 最大边界
  size_t   _size_in_bytes; // 总大小

  // 扩展分配
  void* grow(size_t x, AllocFailType alloc_failmode = AllocFailStrategy::EXIT_OOM);

 public:
  Arena(MEMFLAGS memflag);
  Arena(MEMFLAGS memflag, size_t init_size);
  ~Arena();

  void destruct_contents();

  // 内存分配
  void* Amalloc(size_t x, AllocFailType alloc_failmode = AllocFailStrategy::EXIT_OOM) {
    x = ARENA_ALIGN(x);
    
    if (_hwm + x > _max) {
      return grow(x, alloc_failmode);
    } else {
      char* old = _hwm;
      _hwm += x;
      return old;
    }
  }

  // 4字节对齐分配
  void* Amalloc_4(size_t x, AllocFailType alloc_failmode = AllocFailStrategy::EXIT_OOM) {
    assert((x & (sizeof(char*) - 1)) == 0, "misaligned size");
    return Amalloc(x, alloc_failmode);
  }

  // 快速释放（仅当是最后分配的块时有效）
  void Afree(void* ptr, size_t size) {
    if (ptr == nullptr) return;
    if (((char*)ptr) + ARENA_ALIGN(size) == _hwm) {
      _hwm = (char*)ptr;
    }
  }

  // 重分配
  void* Arealloc(void* old_ptr, size_t old_size, size_t new_size,
                 AllocFailType alloc_failmode = AllocFailStrategy::EXIT_OOM);

  // 查询
  char* hwm() const { return _hwm; }
  size_t used() const;
  size_t size_in_bytes() const { return _size_in_bytes; }
  bool contains(const void* ptr) const;

 private:
  void reset() {
    _first = _chunk = nullptr;
    _hwm = _max = nullptr;
    _size_in_bytes = 0;
  }
};

// ========== 便捷宏 ==========

#define NEW_ARENA_ARRAY(arena, type, size) \
  (type*)(arena)->Amalloc((size) * sizeof(type))

#define FREE_ARENA_ARRAY(arena, type, old, size) \
  (arena)->Afree((char*)(old), (size) * sizeof(type))

#endif // MY_JVM_MEMORY_ARENA_HPP
