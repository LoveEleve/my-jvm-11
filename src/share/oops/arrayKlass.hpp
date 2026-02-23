#ifndef SHARE_OOPS_ARRAYKLASS_HPP
#define SHARE_OOPS_ARRAYKLASS_HPP

// ============================================================================
// Mini JVM - ArrayKlass（数组类元数据基类）
// 对应 HotSpot: src/hotspot/share/oops/arrayKlass.hpp
//
// ArrayKlass 是所有数组类的基类（TypeArrayKlass 和 ObjArrayKlass 的公共父类）。
//
// Klass 层次：
//   Klass
//   ├── InstanceKlass (普通类)
//   └── ArrayKlass (数组类基类)
//       ├── TypeArrayKlass (int[], byte[] 等)
//       └── ObjArrayKlass (Object[], String[] 等)
//
// ArrayKlass 主要字段：
//   _dimension: 数组维度（一维=1，二维=2，...）
//   _higher_dimension: 更高维度的数组类（int[] → int[][]）
//   _lower_dimension: 更低维度的数组类（int[][] → int[]）
// ============================================================================

#include "oops/klass.hpp"

class ArrayKlass : public Klass {
protected:
    int    _dimension;         // 数组维度
    Klass* _higher_dimension;  // 更高维度的数组类
    Klass* _lower_dimension;   // 更低维度的数组类

public:
    ArrayKlass(KlassID id)
        : Klass(id),
          _dimension(1),
          _higher_dimension(nullptr),
          _lower_dimension(nullptr)
    {}

    virtual ~ArrayKlass() {}

    // 维度
    int dimension() const { return _dimension; }
    void set_dimension(int d) { _dimension = d; }

    // 高/低维度链接
    Klass* higher_dimension() const { return _higher_dimension; }
    void set_higher_dimension(Klass* k) { _higher_dimension = k; }

    Klass* lower_dimension() const { return _lower_dimension; }
    void set_lower_dimension(Klass* k) { _lower_dimension = k; }

    // Metadata 虚函数
    const char* internal_name() const override { return "ArrayKlass"; }
};

#endif // SHARE_OOPS_ARRAYKLASS_HPP
