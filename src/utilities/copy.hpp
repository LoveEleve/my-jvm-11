/*
 * my_jvm - Copy utilities
 * 
 * 内存复制工具
 */

#ifndef MY_JVM_UTILITIES_COPY_HPP
#define MY_JVM_UTILITIES_COPY_HPP

#include "utilities/globalDefinitions.hpp"
#include <cstring>

// ========== Copy 类声明 ==========

class Copy {
public:
  static void conjoint_memory(void* from, void* to, size_t size);
  static void conjoint_jbytes(void* from, void* to, size_t count);
  static void conjoint_jshorts(jshort* from, jshort* to, size_t count);
  static void conjoint_jints(jint* from, jint* to, size_t count);
  static void conjoint_jlongs(jlong* from, jlong* to, size_t count);
  static void conjoint_oops(void** from, void** to, size_t count);
  static void arrayof_conjoint_jbytes(HeapWord* from, HeapWord* to, size_t count);
  static void arrayof_conjoint_jshorts(HeapWord* from, HeapWord* to, size_t count);
  static void arrayof_conjoint_jints(HeapWord* from, HeapWord* to, size_t count);
  static void arrayof_conjoint_jlongs(HeapWord* from, HeapWord* to, size_t count);
  static void fill_to_memory(void* to, size_t size, jubyte value);
  static void fill_to_words(HeapWord* to, size_t count, julong value);
  static void fill_to_bytes(void* to, size_t count, jubyte value);
};

// ========== 内存复制实现 ==========

inline void Copy::conjoint_memory(void* from, void* to, size_t size) {
  memmove(to, from, size);
}

inline void Copy::conjoint_jbytes(void* from, void* to, size_t count) {
  memmove(to, from, count);
}

inline void Copy::conjoint_jshorts(jshort* from, jshort* to, size_t count) {
  memmove(to, from, count * sizeof(jshort));
}

inline void Copy::conjoint_jints(jint* from, jint* to, size_t count) {
  memmove(to, from, count * sizeof(jint));
}

inline void Copy::conjoint_jlongs(jlong* from, jlong* to, size_t count) {
  memmove(to, from, count * sizeof(jlong));
}

inline void Copy::conjoint_oops(void** from, void** to, size_t count) {
  memmove(to, from, count * sizeof(void*));
}

inline void Copy::arrayof_conjoint_jbytes(HeapWord* from, HeapWord* to, size_t count) {
  memmove(to, from, count);
}

inline void Copy::arrayof_conjoint_jshorts(HeapWord* from, HeapWord* to, size_t count) {
  memmove(to, from, count * sizeof(jshort));
}

inline void Copy::arrayof_conjoint_jints(HeapWord* from, HeapWord* to, size_t count) {
  memmove(to, from, count * sizeof(jint));
}

inline void Copy::arrayof_conjoint_jlongs(HeapWord* from, HeapWord* to, size_t count) {
  memmove(to, from, count * sizeof(jlong));
}

inline void Copy::fill_to_memory(void* to, size_t size, jubyte value) {
  memset(to, value, size);
}

inline void Copy::fill_to_words(HeapWord* to, size_t count, julong value) {
  for (size_t i = 0; i < count; i++) {
    ((julong*)to)[i] = value;
  }
}

inline void Copy::fill_to_bytes(void* to, size_t count, jubyte value) {
  memset(to, value, count);
}

#endif // MY_JVM_UTILITIES_COPY_HPP
