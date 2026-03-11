/*
 * my_jvm - Arena implementation
 * 
 * 参考 OpenJDK 11 hotspot/src/hotspot/share/memory/arena.cpp
 */

#include "memory/arena.hpp"
#include <cstring>

// ========== Arena 实现 ==========

Arena::Arena(MEMFLAGS memflag) 
  : _flags(memflag), 
    _first(nullptr), 
    _chunk(nullptr),
    _hwm(nullptr), 
    _max(nullptr),
    _size_in_bytes(0) {
}

Arena::Arena(MEMFLAGS memflag, size_t init_size)
  : _flags(memflag),
    _first(nullptr),
    _chunk(nullptr),
    _hwm(nullptr),
    _max(nullptr),
    _size_in_bytes(0) {
  // 预分配初始 Chunk
  if (init_size > 0) {
    Chunk* k = new(init_size) Chunk(init_size);
    _first = _chunk = k;
    _hwm = k->bottom();
    _max = k->top();
    _size_in_bytes = init_size + Chunk::aligned_overhead_size();
  }
}

Arena::~Arena() {
  destruct_contents();
}

void Arena::destruct_contents() {
  // 释放所有 Chunk
  Chunk* k = _first;
  while (k != nullptr) {
    Chunk* next = k->next();
    delete k;
    k = next;
  }
  reset();
}

void* Arena::grow(size_t x, AllocFailType alloc_failmode) {
  // 计算新 Chunk 大小：至少是请求大小的两倍，或者默认大小
  size_t len = x * 2;
  if (len < Chunk::size) {
    len = Chunk::size;
  }
  
  // 分配新 Chunk
  Chunk* k = new(len) Chunk(len);
  if (k == nullptr) {
    if (alloc_failmode == AllocFailStrategy::RETURN_NULL) {
      return nullptr;
    }
    fprintf(stderr, "Arena::grow out of memory\n");
    std::abort();
  }
  
  _size_in_bytes += len + Chunk::aligned_overhead_size();
  
  // 链接到 Chunk 链表
  if (_first == nullptr) {
    _first = k;
  } else {
    _chunk->set_next(k);
  }
  _chunk = k;
  _hwm = k->bottom();
  _max = k->top();
  
  // 分配请求的内存
  char* old = _hwm;
  _hwm += x;
  return old;
}

void* Arena::Arealloc(void* old_ptr, size_t old_size, size_t new_size,
                      AllocFailType alloc_failmode) {
  if (old_ptr == nullptr) {
    return Amalloc(new_size, alloc_failmode);
  }
  
  old_size = ARENA_ALIGN(old_size);
  new_size = ARENA_ALIGN(new_size);
  
  // 如果新大小 <= 旧大小，直接返回原指针
  if (new_size <= old_size) {
    return old_ptr;
  }
  
  // 检查是否可以原地扩展（是最后分配的块）
  if (((char*)old_ptr) + old_size == _hwm && 
      _hwm + (new_size - old_size) <= _max) {
    _hwm += (new_size - old_size);
    return old_ptr;
  }
  
  // 需要重新分配
  void* new_ptr = Amalloc(new_size, alloc_failmode);
  if (new_ptr == nullptr) {
    return nullptr;
  }
  
  // 复制旧数据
  memcpy(new_ptr, old_ptr, old_size);
  
  // 释放旧内存（如果可能）
  Afree(old_ptr, old_size);
  
  return new_ptr;
}

size_t Arena::used() const {
  size_t sum = 0;
  for (Chunk* k = _first; k != nullptr; k = k->next()) {
    if (k == _chunk) {
      sum += _hwm - k->bottom();
    } else {
      sum += k->length();
    }
  }
  return sum;
}

bool Arena::contains(const void* ptr) const {
  for (Chunk* k = _first; k != nullptr; k = k->next()) {
    if (k->contains((char*)ptr)) {
      return true;
    }
  }
  return false;
}
