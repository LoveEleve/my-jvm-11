#ifndef SHARE_OOPS_OOPSHIERARCHY_HPP
#define SHARE_OOPS_OOPSHIERARCHY_HPP

// ============================================================================
// Mini JVM - OOP 类型层次
// 对应 HotSpot: src/hotspot/share/oops/oopsHierarchy.hpp
//
// OOP = Ordinary Object Pointer，即 Java 对象在 JVM 内部的指针表示。
//
// HotSpot 的对象模型核心思想：
//   - oop (oopDesc*)  指向 Java 堆中的对象实例
//   - Klass*          指向元空间中的类元数据
//   - 每个 oop 的对象头中都有一个指向 Klass 的指针
//
// oop 类型层次：
//   oop            → oopDesc*           (所有对象的基类指针)
//   instanceOop    → instanceOopDesc*   (普通 Java 实例)
//   arrayOop       → arrayOopDesc*      (数组基类)
//   objArrayOop    → objArrayOopDesc*   (对象数组，如 Object[])
//   typeArrayOop   → typeArrayOopDesc*  (原始类型数组，如 int[])
//
// Klass 类型层次：
//   Klass
//   ├── InstanceKlass              (普通 Java 类)
//   │   ├── InstanceMirrorKlass    (java.lang.Class 类)
//   │   ├── InstanceClassLoaderKlass (ClassLoader 类)
//   │   └── InstanceRefKlass       (Reference 类)
//   └── ArrayKlass
//       ├── ObjArrayKlass          (对象数组)
//       └── TypeArrayKlass         (原始数组)
// ============================================================================

#include "utilities/globalDefinitions.hpp"

// ----------------------------------------------------------------------------
// 压缩指针类型
// HotSpot: oopsHierarchy.hpp 头部
//
// narrowOop: 压缩对象指针，32 位偏移量
//   实际地址 = base + (narrowOop << shift)
//   这样 4 字节就能寻址 32GB 堆空间
// narrowKlass: 压缩类指针，类似原理
// ----------------------------------------------------------------------------

typedef juint  narrowOop;          // 压缩对象指针（32位）
typedef juint  narrowKlass;        // 压缩类指针（32位）

// ----------------------------------------------------------------------------
// markOop — 对象头的 mark word
// 定义在 markOop.hpp 中。markOop = markOopDesc*，是一个被当作位域使用的指针。
// 这里只做前向声明，完整定义在 markOop.hpp。
// ----------------------------------------------------------------------------

class markOopDesc;
typedef markOopDesc* markOop;

// ----------------------------------------------------------------------------
// OOP 类型定义
// HotSpot: oopsHierarchy.hpp 约 30-50 行（非 CHECK_UNHANDLED_OOPS 模式）
//
// 简单模式下就是 typedef，直接把 xxxOopDesc* 起个短名字。
// 我们用简单模式。
// ----------------------------------------------------------------------------

// 前向声明 oopDesc 家族
class oopDesc;
class instanceOopDesc;
class arrayOopDesc;
class objArrayOopDesc;
class typeArrayOopDesc;

// oop 类型 = 对应 Desc 类的指针
typedef oopDesc*          oop;           // 基础对象指针
typedef instanceOopDesc*  instanceOop;   // Java 实例对象
typedef arrayOopDesc*     arrayOop;      // 数组基类
typedef objArrayOopDesc*  objArrayOop;   // 对象数组 (Object[])
typedef typeArrayOopDesc* typeArrayOop;  // 原始数组 (int[], byte[] 等)

// 空 oop
#define NULL_OOP  ((oop)nullptr)

// ----------------------------------------------------------------------------
// Klass 家族前向声明
// ----------------------------------------------------------------------------

class Klass;
class InstanceKlass;
class InstanceMirrorKlass;
class InstanceClassLoaderKlass;
class InstanceRefKlass;
class ArrayKlass;
class ObjArrayKlass;
class TypeArrayKlass;

// ----------------------------------------------------------------------------
// Metadata 家族前向声明
// ----------------------------------------------------------------------------

class Method;
class ConstMethod;
class ConstantPool;
class ConstantPoolCache;

// ----------------------------------------------------------------------------
// 类型转换辅助
// ----------------------------------------------------------------------------

template <class T>
inline oop cast_to_oop(T value) {
    return (oop)(void*)value;
}

template <class T>
inline T cast_from_oop(oop o) {
    return (T)(void*)o;
}

// 检查 oop 对齐（对象必须 8 字节对齐）
inline bool check_obj_alignment(oop obj) {
    return is_aligned((uintx)obj, (uintx)HeapWordSize);
}

#endif // SHARE_OOPS_OOPSHIERARCHY_HPP
