/*
 * my_jvm - Memory allocation implementation
 */

#include "memory/allocation.hpp"
#include "memory/resourceArea.hpp"

// ========== ResourceObj ==========

void* ResourceObj::operator new(size_t size) {
  return resource_allocate_bytes(size);
}

void ResourceObj::operator delete(void* p) {
  // ResourceArea 不支持单独释放，忽略
}
