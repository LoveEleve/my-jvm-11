/*
 * my_jvm - Resource Area implementation
 */

#include "memory/resourceArea.hpp"

// 全局 ResourceArea（简化版）
static ResourceArea _shared_resource_area(mtInternal);

ResourceArea* current_resource_area() {
  return &_shared_resource_area;
}
