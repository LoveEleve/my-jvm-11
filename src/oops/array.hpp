/*
 * my_jvm - Array template for metadata
 *
 * 元数据分配的数组模板，来自 OpenJDK 11 hotspot/src/share/vm/oops/array.hpp
 * 简化版本
 */

#ifndef MY_JVM_OOPS_ARRAY_HPP
#define MY_JVM_OOPS_ARRAY_HPP

#include "globalDefinitions.hpp"
#include "metadata.hpp"

// ========== Array 模板类 ==========
// 参考：array.hpp 第 36-155 行
// 用于存储 Metaspace 中的元数据数组（如 Method* 数组）

template <typename T>
class Array : public MetaspaceObj {
private:
    int _length;                        // 数组元素数量
    T   _data[1];                       // 数组内存（变长数组）

public:
    // ========== 基本操作 ==========

    int length() const { return _length; }
    bool is_empty() const { return _length == 0; }

    // 获取原始数据指针
    T* data() { return _data; }
    const T* data() const { return _data; }

    // ========== 元素访问 ==========

    T at(int i) const {
        // 简化版省略 assert，实际应该有边界检查
        return _data[i];
    }

    void at_put(int i, T x) {
        _data[i] = x;
    }

    T* adr_at(int i) {
        return &_data[i];
    }

    // ========== 查找 ==========

    int index_of(const T& x) const {
        int i = _length;
        while (i-- > 0 && _data[i] != x);
        return i;
    }

    bool contains(const T& x) const {
        return index_of(x) >= 0;
    }
};

#endif // MY_JVM_OOPS_ARRAY_HPP
