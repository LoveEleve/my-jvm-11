#ifndef SHARE_OOPS_TYPEARRAYKLASS_HPP
#define SHARE_OOPS_TYPEARRAYKLASS_HPP

// ============================================================================
// Mini JVM - TypeArrayKlass（基本类型数组的类元数据）
// 对应 HotSpot: src/hotspot/share/oops/typeArrayKlass.hpp
//
// TypeArrayKlass 描述基本类型数组的「类」信息。
// 每种基本类型有一个全局唯一的 TypeArrayKlass 实例：
//   T_INT     → [I  → TypeArrayKlass for int[]
//   T_BYTE    → [B  → TypeArrayKlass for byte[]
//   T_CHAR    → [C  → TypeArrayKlass for char[]
//   T_SHORT   → [S  → TypeArrayKlass for short[]
//   T_LONG    → [J  → TypeArrayKlass for long[]
//   T_FLOAT   → [F  → TypeArrayKlass for float[]
//   T_DOUBLE  → [D  → TypeArrayKlass for double[]
//   T_BOOLEAN → [Z  → TypeArrayKlass for boolean[]
//
// newarray 字节码的 atype 参数：
//   T_BOOLEAN = 4, T_CHAR = 5, T_FLOAT = 6, T_DOUBLE = 7,
//   T_BYTE = 8, T_SHORT = 9, T_INT = 10, T_LONG = 11
//
// 核心方法：
//   allocate_array(int length) — 在堆上分配基本类型数组
// ============================================================================

#include "oops/arrayKlass.hpp"
#include "oops/typeArrayOop.hpp"

class TypeArrayKlass : public ArrayKlass {
private:
    BasicType _element_type;     // 元素类型（T_INT, T_BYTE 等）
    int       _element_size;     // 元素大小（字节）
    int       _max_length;       // 最大允许长度

    // 全局 TypeArrayKlass 实例（每种基本类型一个）
    static TypeArrayKlass* _type_array_klasses[T_CONFLICT + 1];

public:
    TypeArrayKlass(BasicType type, int element_size, const char* name);
    ~TypeArrayKlass();

    // 元素类型和大小
    BasicType element_type() const { return _element_type; }
    int element_size() const { return _element_size; }

    // 计算指定长度的数组对象总大小（字节，已对齐）
    int array_size_in_bytes(int length) const {
        size_t data_size = (size_t)length * _element_size;
        size_t total = arrayOopDesc::header_size_in_bytes() + data_size;
        return (int)align_up(total, (size_t)HeapWordSize);
    }

    // 分配数组对象
    typeArrayOopDesc* allocate_array(int length);

    // Metadata 虚函数
    const char* internal_name() const override { return "TypeArrayKlass"; }

    // ======== 全局初始化/访问 ========

    // 初始化所有基本类型的 TypeArrayKlass
    static void initialize_all();

    // 销毁所有
    static void destroy_all();

    // 根据 BasicType 获取对应的 TypeArrayKlass
    static TypeArrayKlass* for_type(BasicType type) {
        if (type >= T_BOOLEAN && type <= T_LONG) {
            return _type_array_klasses[type];
        }
        return nullptr;
    }

    // 根据 newarray 的 atype 参数获取 TypeArrayKlass
    // atype: T_BOOLEAN=4, T_CHAR=5, T_FLOAT=6, T_DOUBLE=7,
    //        T_BYTE=8, T_SHORT=9, T_INT=10, T_LONG=11
    static TypeArrayKlass* for_atype(int atype) {
        return for_type((BasicType)atype);
    }

    // 调试打印
    void print_on(FILE* out) const override {
        fprintf(out, "TypeArrayKlass(%p): name=\"%s\", element_type=%d, element_size=%d",
                (void*)this, name() ? name() : "<null>",
                (int)_element_type, _element_size);
    }

    NONCOPYABLE(TypeArrayKlass);
};

#endif // SHARE_OOPS_TYPEARRAYKLASS_HPP
