#ifndef SHARE_OOPS_TYPEARRAYOOP_HPP
#define SHARE_OOPS_TYPEARRAYOOP_HPP

// ============================================================================
// Mini JVM - typeArrayOopDesc（基本类型数组对象）
// 对应 HotSpot: src/hotspot/share/oops/typeArrayOop.hpp
//
// 表示 int[], byte[], char[], long[], float[], double[] 等基本类型数组。
//
// 继承 arrayOopDesc，增加类型化的元素访问方法。
// 元素存储在 base() 之后的连续内存中。
//
// 示例 int[3] 的内存布局：
//   ┌──────────────────────┐  offset 0
//   │ mark (8B)            │
//   ├──────────────────────┤  offset 8
//   │ klass* (8B)          │  → TypeArrayKlass
//   ├──────────────────────┤  offset 16
//   │ length = 3 (4B)      │
//   ├──────────────────────┤  offset 20
//   │ padding (4B)         │
//   ├──────────────────────┤  offset 24
//   │ element[0] (4B)      │
//   │ element[1] (4B)      │
//   │ element[2] (4B)      │
//   │ [pad to 8B] (4B)     │
//   └──────────────────────┘  total = 40 bytes (aligned)
// ============================================================================

#include "oops/arrayOop.hpp"

class typeArrayOopDesc : public arrayOopDesc {
public:
    // ======== 类型化元素访问 ========
    // 通过 base() + index * element_size 计算元素地址

    // --- boolean/byte ---
    jbyte* byte_base() const { return (jbyte*)base(); }

    jbyte byte_at(int index) const {
        return byte_base()[index];
    }
    void byte_at_put(int index, jbyte value) {
        byte_base()[index] = value;
    }

    // --- char ---
    jchar* char_base() const { return (jchar*)base(); }

    jchar char_at(int index) const {
        return char_base()[index];
    }
    void char_at_put(int index, jchar value) {
        char_base()[index] = value;
    }

    // --- short ---
    jshort* short_base() const { return (jshort*)base(); }

    jshort short_at(int index) const {
        return short_base()[index];
    }
    void short_at_put(int index, jshort value) {
        short_base()[index] = value;
    }

    // --- int ---
    jint* int_base() const { return (jint*)base(); }

    jint int_at(int index) const {
        return int_base()[index];
    }
    void int_at_put(int index, jint value) {
        int_base()[index] = value;
    }

    // --- long ---
    jlong* long_base() const { return (jlong*)base(); }

    jlong long_at(int index) const {
        return long_base()[index];
    }
    void long_at_put(int index, jlong value) {
        long_base()[index] = value;
    }

    // --- float ---
    jfloat* float_base() const { return (jfloat*)base(); }

    jfloat float_at(int index) const {
        return float_base()[index];
    }
    void float_at_put(int index, jfloat value) {
        float_base()[index] = value;
    }

    // --- double ---
    jdouble* double_base() const { return (jdouble*)base(); }

    jdouble double_at(int index) const {
        return double_base()[index];
    }
    void double_at_put(int index, jdouble value) {
        double_base()[index] = value;
    }
};

#endif // SHARE_OOPS_TYPEARRAYOOP_HPP
