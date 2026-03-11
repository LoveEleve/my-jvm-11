/*
 * my_jvm - Metadata definitions
 * 
 * 元数据基类，来自 OpenJDK 11 hotspot/src/share/vm/oops/metadata.hpp
 * 对标 OpenJDK 11 slowdebug 版本（含 NOT_PRODUCT 字段）
 * 
 * 继承体系：MetaspaceObj → Metadata → Klass/Method
 * 
 * 内存布局（64-bit）：
 *   [0]  vtable ptr (8 bytes)
 *   [8]  _valid: int (4 bytes) + padding (4 bytes)
 *   sizeof(Metadata) = 16  ← 与 OpenJDK slowdebug 一致
 */

#ifndef MY_JVM_OOPS_METADATA_HPP
#define MY_JVM_OOPS_METADATA_HPP

#include "globalDefinitions.hpp"

// ========== MetaspaceObj 基类（简化版）==========
// 所有存在于 Metaspace 的对象都继承自这个基类

class MetaspaceObj {
public:
    // MetaspaceObj 是虚基类
    virtual ~MetaspaceObj() {}
    
    // 判断类型（简化版）
    virtual bool is_klass() const { return false; }
    virtual bool is_instance_klass() const { return false; }
    virtual bool is_array_klass() const { return false; }
    virtual bool is_method() const { return false; }
    virtual bool is_constant_pool() const { return false; }
};

// ========== Metadata 类 ==========
// 所有类相关元数据的基类，存在 Metaspace 中
// 
// 内存布局（64-bit slowdebug）：
//   offset 0:  vtable ptr (8 bytes)
//   offset 8:  _valid: int (4 bytes) + 4 bytes padding
//   sizeof = 16
//
// 注意：OpenJDK 源码中 NOT_PRODUCT(int _valid;) 在 slowdebug 版本中存在
// my_jvm 始终包含此字段以对标 OpenJDK slowdebug 的内存布局

class Metadata : public MetaspaceObj {
    // 调试用字段：用于检测 Metadata 是否已被删除
    // 对应 OpenJDK: NOT_PRODUCT(int _valid;)
    // 在 slowdebug 版本中始终存在，sizeof(Metadata) = 16
    int _valid;

public:
    // ========== 构造函数 ==========
    
    Metadata() : _valid(0) {}
    
    // ========== 调试辅助 ==========
    
    bool is_valid() const volatile { return _valid == 0; }
    
    // 获取身份哈希
    int identity_hash() { return (int)(uintptr_t)this; }
    
    // ========== 类型判断 ==========
    
    virtual bool is_metadata() const { return true; }
    virtual bool is_klass() const override { return false; }
    virtual bool is_method() const override { return false; }
    
    // ========== 大小和类型（简化版）==========
    
    virtual int size() const { return 0; }
    
    // ========== 调试打印（简化版）==========
    
    virtual const char* internal_name() const { return "Metadata"; }
};


// ========== 类型别名 ==========

typedef Metadata* MetadataPtr;

#endif // MY_JVM_OOPS_METADATA_HPP
