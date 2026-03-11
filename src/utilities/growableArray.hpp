/*
 * my_jvm - Growable Array
 * 
 * 参考 OpenJDK 11 hotspot/src/hotspot/share/utilities/growableArray.hpp
 * 简化版本：去除 oop/ostream 依赖
 */

#ifndef MY_JVM_UTILITIES_GROWABLEARRAY_HPP
#define MY_JVM_UTILITIES_GROWABLEARRAY_HPP

#include "memory/allocation.hpp"
#include "utilities/debug.hpp"
#include "utilities/globalDefinitions.hpp"
#include <new>
#include <cstring>

// ========== GenericGrowableArray 基类 ==========

class GenericGrowableArray : public ResourceObj {
 protected:
  int    _len;       // 当前长度
  int    _max;       // 最大容量
  Arena* _arena;     // 分配位置：NULL=ResourceArea, 1=CHeap, 其他=指定Arena
  MEMFLAGS _memflags; // CHeap 时的内存类型

  bool on_C_heap() const { return _arena == (Arena*)1; }
  bool on_stack()  const { return _arena == nullptr; }
  bool on_arena()  const { return _arena > (Arena*)1; }

  GenericGrowableArray(int initial_size, int initial_len, bool c_heap, 
                       MEMFLAGS flags = mtNone) {
    _len = initial_len;
    _max = initial_size;
    _memflags = flags;
    _arena = c_heap ? (Arena*)1 : nullptr;
    assert(_len >= 0 && _len <= _max, "initial_len too big");
  }

  GenericGrowableArray(Arena* arena, int initial_size, int initial_len) {
    _len = initial_len;
    _max = initial_size;
    _arena = arena;
    _memflags = mtNone;
    assert(_len >= 0 && _len <= _max, "initial_len too big");
  }

  void* raw_allocate(int elementSize);
  void free_C_heap(void* elements);
};

// ========== GrowableArray 模板 ==========

template<class E>
class GrowableArray : public GenericGrowableArray {
 private:
  E* _data;

  void grow(int j) {
    int new_max = _max;
    if (new_max == 0) new_max = 2;
    while (new_max <= j) new_max = new_max * 2;
    
    E* new_data;
    if (on_C_heap()) {
      new_data = (E*)AllocateHeap(new_max * sizeof(E), _memflags);
    } else if (on_arena()) {
      new_data = (E*)_arena->Amalloc(new_max * sizeof(E));
    } else {
      new_data = (E*)resource_allocate_bytes(new_max * sizeof(E));
    }
    
    // 复制旧数据
    for (int i = 0; i < _len; i++) {
      ::new ((void*)&new_data[i]) E(_data[i]);
    }
    
    // 初始化新元素
    for (int i = _len; i < new_max; i++) {
      ::new ((void*)&new_data[i]) E();
    }
    
    // 释放旧数据
    if (on_C_heap() && _data != nullptr) {
      for (int i = 0; i < _max; i++) {
        _data[i].~E();
      }
      FreeHeap(_data);
    }
    
    _data = new_data;
    _max = new_max;
  }

 public:
  GrowableArray(int initial_size, bool c_heap = false, MEMFLAGS flags = mtInternal)
    : GenericGrowableArray(initial_size, 0, c_heap, flags) {
    _data = (E*)raw_allocate(sizeof(E));
    for (int i = 0; i < _max; i++) {
      ::new ((void*)&_data[i]) E();
    }
  }

  GrowableArray(Arena* arena, int initial_size, int initial_len, const E& filler)
    : GenericGrowableArray(arena, initial_size, initial_len) {
    _data = (E*)raw_allocate(sizeof(E));
    for (int i = 0; i < _len; i++) {
      ::new ((void*)&_data[i]) E(filler);
    }
    for (int i = _len; i < _max; i++) {
      ::new ((void*)&_data[i]) E();
    }
  }

  ~GrowableArray() {
    if (on_C_heap()) {
      clear_and_deallocate();
    }
  }

  void clear_and_deallocate() {
    if (_data != nullptr) {
      for (int i = 0; i < _max; i++) {
        _data[i].~E();
      }
      if (on_C_heap()) {
        FreeHeap(_data);
      }
      _data = nullptr;
    }
    _len = 0;
    _max = 0;
  }

  // 访问
  E& at(int i) {
    assert(0 <= i && i < _len, "index out of bounds");
    return _data[i];
  }

  const E& at(int i) const {
    assert(0 <= i && i < _len, "index out of bounds");
    return _data[i];
  }

  E& operator[](int i) { return at(i); }
  const E& operator[](int i) const { return at(i); }

  E* adr_at(int i) {
    assert(0 <= i && i < _len, "index out of bounds");
    return &_data[i];
  }

  // 查询
  int length() const { return _len; }
  int max_length() const { return _max; }
  bool is_empty() const { return _len == 0; }
  bool is_nonempty() const { return _len > 0; }

  // 修改
  void append(const E& e) {
    if (_len == _max) grow(_len);
    ::new ((void*)&_data[_len]) E(e);
    _len++;
  }

  void push(const E& e) { append(e); }

  E pop() {
    assert(!is_empty(), "empty list");
    _len--;
    return _data[_len];
  }

  E top() const {
    assert(!is_empty(), "empty list");
    return _data[_len - 1];
  }

  void at_put(int i, const E& e) {
    assert(0 <= i && i < _len, "index out of bounds");
    _data[i] = e;
  }

  void at_put_grow(int i, const E& e, const E& fill) {
    if (i >= _len) {
      while (i >= _max) grow(i);
      for (int j = _len; j < i; j++) {
        ::new ((void*)&_data[j]) E(fill);
      }
      _len = i + 1;
    }
    _data[i] = e;
  }

  void clear() { _len = 0; }

  void trunc_to(int length) {
    assert(0 <= length && length <= _len, "length out of bounds");
    _len = length;
  }

  // 查找
  int find(const E& e) const {
    for (int i = 0; i < _len; i++) {
      if (_data[i] == e) return i;
    }
    return -1;
  }

  int find_from(int idx, const E& e) const {
    for (int i = idx; i < _len; i++) {
      if (_data[i] == e) return i;
    }
    return -1;
  }

  bool contains(const E& e) const {
    return find(e) >= 0;
  }

  // 删除
  void remove_at(int i) {
    assert(0 <= i && i < _len, "index out of bounds");
    for (int j = i; j < _len - 1; j++) {
      _data[j] = _data[j + 1];
    }
    _len--;
  }

  void remove(const E& e) {
    int idx = find(e);
    if (idx >= 0) remove_at(idx);
  }

  // 迭代器支持
  E* begin() { return _data; }
  E* end() { return _data + _len; }
  const E* begin() const { return _data; }
  const E* end() const { return _data + _len; }
};

// ========== GenericGrowableArray 实现 ==========

inline void* GenericGrowableArray::raw_allocate(int elementSize) {
  void* p;
  if (on_C_heap()) {
    p = AllocateHeap(elementSize * _max, _memflags);
  } else if (on_arena()) {
    p = _arena->Amalloc(elementSize * _max);
  } else {
    p = resource_allocate_bytes(elementSize * _max);
  }
  return p;
}

inline void GenericGrowableArray::free_C_heap(void* elements) {
  FreeHeap(elements);
}

#endif // MY_JVM_UTILITIES_GROWABLEARRAY_HPP
