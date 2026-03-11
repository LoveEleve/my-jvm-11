/*
 * my_jvm - Resource Area (简化版)
 * 
 * 参考 OpenJDK 11 hotspot/src/hotspot/share/memory/resourceArea.hpp
 * 简化版本：去除 Thread 依赖，只保留 Arena 状态保存/恢复
 */

#ifndef MY_JVM_MEMORY_RESOURCEAREA_HPP
#define MY_JVM_MEMORY_RESOURCEAREA_HPP

#include "memory/arena.hpp"

// ========== ResourceArea ==========

class ResourceArea : public Arena {
  friend class ResourceMark;

 public:
  ResourceArea(MEMFLAGS flags = mtThread) : Arena(flags) {}
  ResourceArea(size_t init_size, MEMFLAGS flags = mtThread) 
    : Arena(flags, init_size) {}

  char* allocate_bytes(size_t size, 
                       AllocFailType alloc_failmode = AllocFailStrategy::EXIT_OOM) {
    return (char*)Amalloc(size, alloc_failmode);
  }
};

// ========== ResourceMark ==========

class ResourceMark : public StackObj {
 protected:
  ResourceArea* _area;       // 关联的 ResourceArea
  Chunk*        _chunk;      // 保存的 Chunk
  char*         _hwm;        // 保存的高水位
  char*         _max;        // 保存的最大边界
  size_t        _size_in_bytes; // 保存的总大小

 public:
  ResourceMark(ResourceArea* r) 
    : _area(r), 
      _chunk(r->_chunk), 
      _hwm(r->_hwm), 
      _max(r->_max),
      _size_in_bytes(r->_size_in_bytes) {}

  ~ResourceMark() {
    reset_to_mark();
  }

  void reset_to_mark() {
    // 释放后续 Chunk
    if (_chunk != nullptr && _chunk->next() != nullptr) {
      Chunk* k = _chunk->next();
      while (k != nullptr) {
        Chunk* next = k->next();
        delete k;
        k = next;
      }
      _chunk->set_next(nullptr);
    }
    
    // 恢复 Arena 状态
    _area->_chunk = _chunk;
    _area->_hwm = _hwm;
    _area->_max = _max;
    _area->_size_in_bytes = _size_in_bytes;
  }
};

// ========== 便捷宏 ==========

#define NEW_RESOURCE_ARRAY(type, size) \
  (type*) resource_allocate_bytes((size) * sizeof(type))

#define NEW_RESOURCE_OBJ(type) \
  NEW_RESOURCE_ARRAY(type, 1)

// 全局 ResourceArea（简化版）
extern ResourceArea* current_resource_area();

inline void* resource_allocate_bytes(size_t size, 
                                     AllocFailType alloc_failmode = AllocFailStrategy::EXIT_OOM) {
  return current_resource_area()->Amalloc(size, alloc_failmode);
}

#endif // MY_JVM_MEMORY_RESOURCEAREA_HPP
