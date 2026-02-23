#ifndef SHARE_OOPS_METADATA_HPP
#define SHARE_OOPS_METADATA_HPP

// ============================================================================
// Mini JVM - Metadata 基类
// 对应 HotSpot: src/hotspot/share/oops/metadata.hpp
//
// Metadata 是所有类元数据对象的基类。
// 在 HotSpot 中，Metadata 继承自 MetaspaceObj（元空间分配的对象）。
//
// 继承层次：
//   MetaspaceObj (元空间分配)
//     └── Metadata (元数据基类)
//           ├── Klass         (类元数据)
//           │   └── InstanceKlass, ArrayKlass ...
//           ├── Method        (方法元数据)
//           └── ConstantPool  (常量池)
//
// Metadata 提供类型判断虚函数：is_klass(), is_method() 等。
// 这是 oop/Klass 分离设计的关键：
//   - oop (oopDesc) 没有虚函数表（为了节省对象头空间）
//   - Klass (Metadata) 有虚函数表（提供多态行为）
//
// 我们的精简版：
//   - 暂不实现 MetaspaceObj（后续引入 Metaspace 时再加）
//   - 暂用 CHeapObj<mtClass> 代替 MetaspaceObj
//   - 保留虚函数用于类型判断
// ============================================================================

#include "memory/allocation.hpp"

class Metadata : public CHeapObj<mtClass> {
public:
    Metadata() {}
    virtual ~Metadata() {}

    // 类型判断虚函数
    virtual bool is_metadata()     const { return true; }
    virtual bool is_klass()        const { return false; }
    virtual bool is_method()       const { return false; }
    virtual bool is_constantPool() const { return false; }

    // 内部名称（调试用）
    virtual const char* internal_name() const = 0;

    // 调试打印
    virtual void print_on(FILE* out) const {
        fprintf(out, "Metadata(%p) [%s]", (void*)this, internal_name());
    }
};

#endif // SHARE_OOPS_METADATA_HPP
