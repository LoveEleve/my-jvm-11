/*
 * my_jvm - Atomic operations
 * 
 * 原子操作封装，来自 OpenJDK 11 hotspot/src/share/vm/runtime/atomic.hpp
 * 简化版本
 */

#ifndef MY_JVM_RUNTIME_ATOMIC_HPP
#define MY_JVM_RUNTIME_ATOMIC_HPP

#include "globalDefinitions.hpp"

// ========== 原子加载 ==========

inline jint atomic_load(const jint* addr) {
    return __atomic_load_n(addr, __ATOMIC_SEQ_CST);
}

inline juint atomic_load(const juint* addr) {
    return __atomic_load_n(addr, __ATOMIC_SEQ_CST);
}

inline intptr_t atomic_load(const intptr_t* addr) {
    return __atomic_load_n(addr, __ATOMIC_SEQ_CST);
}

inline uintptr_t atomic_load(const uintptr_t* addr) {
    return __atomic_load_n(addr, __ATOMIC_SEQ_CST);
}

inline void* atomic_load(void* const* addr) {
    return __atomic_load_n(addr, __ATOMIC_SEQ_CST);
}

// ========== 原子存储 ==========

inline void atomic_store(jint* addr, jint val) {
    __atomic_store_n(addr, val, __ATOMIC_SEQ_CST);
}

inline void atomic_store(juint* addr, juint val) {
    __atomic_store_n(addr, val, __ATOMIC_SEQ_CST);
}

inline void atomic_store(intptr_t* addr, intptr_t val) {
    __atomic_store_n(addr, val, __ATOMIC_SEQ_CST);
}

inline void atomic_store(uintptr_t* addr, uintptr_t val) {
    __atomic_store_n(addr, val, __ATOMIC_SEQ_CST);
}

inline void atomic_store(void** addr, void* val) {
    __atomic_store_n(addr, val, __ATOMIC_SEQ_CST);
}

// ========== CAS（Compare And Swap） ==========

// 返回旧值
inline jint atomic_cas(jint* addr, jint exchange_val, jint compare_val) {
    __atomic_compare_exchange_n(addr, &compare_val, &exchange_val, false, 
                                __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return compare_val;
}

inline juint atomic_cas(juint* addr, juint exchange_val, juint compare_val) {
    __atomic_compare_exchange_n(addr, &compare_val, &exchange_val, false, 
                                __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return compare_val;
}

inline intptr_t atomic_cas(intptr_t* addr, intptr_t exchange_val, intptr_t compare_val) {
    __atomic_compare_exchange_n(addr, &compare_val, &exchange_val, false, 
                                __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return compare_val;
}

inline uintptr_t atomic_cas(uintptr_t* addr, uintptr_t exchange_val, uintptr_t compare_val) {
    __atomic_compare_exchange_n(addr, &compare_val, &exchange_val, false, 
                                __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return compare_val;
}

inline void* atomic_cas(void** addr, void* exchange_val, void* compare_val) {
    __atomic_compare_exchange_n(addr, &compare_val, &exchange_val, false, 
                                __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return compare_val;
}

// ========== 原子加法 ==========

inline jint atomic_add(jint* addr, jint val) {
    return __atomic_fetch_add(addr, val, __ATOMIC_SEQ_CST);
}

inline juint atomic_add(juint* addr, juint val) {
    return __atomic_fetch_add(addr, val, __ATOMIC_SEQ_CST);
}

inline intptr_t atomic_add(intptr_t* addr, intptr_t val) {
    return __atomic_fetch_add(addr, val, __ATOMIC_SEQ_CST);
}

inline uintptr_t atomic_add(uintptr_t* addr, uintptr_t val) {
    return __atomic_fetch_add(addr, val, __ATOMIC_SEQ_CST);
}

// ========== 原子交换 ==========

inline jint atomic_xchg(jint* addr, jint val) {
    return __atomic_exchange_n(addr, val, __ATOMIC_SEQ_CST);
}

inline juint atomic_xchg(juint* addr, juint val) {
    return __atomic_exchange_n(addr, val, __ATOMIC_SEQ_CST);
}

inline intptr_t atomic_xchg(intptr_t* addr, intptr_t val) {
    return __atomic_exchange_n(addr, val, __ATOMIC_SEQ_CST);
}

inline uintptr_t atomic_xchg(uintptr_t* addr, uintptr_t val) {
    return __atomic_exchange_n(addr, val, __ATOMIC_SEQ_CST);
}

inline void* atomic_xchg(void** addr, void* val) {
    return __atomic_exchange_n(addr, val, __ATOMIC_SEQ_CST);
}

#endif // MY_JVM_RUNTIME_ATOMIC_HPP
