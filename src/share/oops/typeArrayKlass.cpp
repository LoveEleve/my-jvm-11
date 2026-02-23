// ============================================================================
// Mini JVM - TypeArrayKlass 实现
// 对应 HotSpot: src/hotspot/share/oops/typeArrayKlass.cpp
//
// 负责基本类型数组的创建和内存分配。
// ============================================================================

#include "oops/typeArrayKlass.hpp"
#include "gc/shared/javaHeap.hpp"
#include "oops/arrayOop.hpp"

#include <cstring>

// 全局 TypeArrayKlass 数组
TypeArrayKlass* TypeArrayKlass::_type_array_klasses[T_CONFLICT + 1] = { nullptr };

// ============================================================================
// 构造/析构
// ============================================================================

TypeArrayKlass::TypeArrayKlass(BasicType type, int element_size, const char* klass_name)
    : ArrayKlass(TypeArrayKlassID),
      _element_type(type),
      _element_size(element_size),
      _max_length(0)
{
    set_name(klass_name);

    // 计算最大长度（防止溢出）
    // 最大分配 = 2GB → max_length = (2GB - header) / element_size
    size_t max_bytes = (size_t)2 * 1024 * 1024 * 1024;
    _max_length = (int)((max_bytes - arrayOopDesc::header_size_in_bytes()) / element_size);

    // 设置 layout_helper 为负数（标记为数组类）
    // 简化版：用 -1 表示数组
    set_layout_helper(-1);
}

TypeArrayKlass::~TypeArrayKlass() {
    // name 是静态字符串，不需要释放
}

// ============================================================================
// allocate_array — 在堆上分配基本类型数组
//
// 对应 HotSpot: TypeArrayKlass::allocate_common()
//   → CollectedHeap::array_allocate()
//     → MemAllocator::allocate()
//       → mem_allocate() + finish()
//
// 流程：
//   1. 检查长度合法性
//   2. 计算总大小 = header + length * element_size（对齐到 8B）
//   3. 从 JavaHeap 分配内存
//   4. 清零 + 设置 mark word + klass + length
//   5. 返回 typeArrayOopDesc*
// ============================================================================

typeArrayOopDesc* TypeArrayKlass::allocate_array(int length) {
    // 负长度检查
    if (length < 0) {
        fprintf(stderr, "ERROR: NegativeArraySizeException: %d\n", length);
        return nullptr;
    }

    // 溢出检查
    if (length > _max_length) {
        fprintf(stderr, "ERROR: OutOfMemoryError: Requested array size exceeds VM limit\n");
        return nullptr;
    }

    // 计算总大小
    int size_in_bytes = array_size_in_bytes(length);
    size_t aligned_size = align_up((size_t)size_in_bytes, (size_t)HeapWordSize);
    size_t size_in_words = aligned_size / HeapWordSize;

    // 从堆分配
    JavaHeap* heap = JavaHeap::heap();
    if (heap == nullptr) {
        fprintf(stderr, "ERROR: JavaHeap not initialized\n");
        return nullptr;
    }

    HeapWord* mem = heap->allocate(size_in_words);
    if (mem == nullptr) {
        fprintf(stderr, "ERROR: OutOfMemoryError: Java heap space (array)\n");
        return nullptr;
    }

    // 清零
    memset(mem, 0, aligned_size);

    // 转为 typeArrayOopDesc*
    typeArrayOopDesc* array = (typeArrayOopDesc*)mem;

    // 设置对象头
    array->set_mark(markOopDesc::prototype());
    array->set_klass(this);

    // 设置长度
    array->set_length(length);

    return array;
}

// ============================================================================
// 全局初始化/销毁
//
// 对应 HotSpot: Universe::genesis() 中调用 TypeArrayKlass::create_klass()
// 为每种基本类型创建一个全局 TypeArrayKlass
// ============================================================================

void TypeArrayKlass::initialize_all() {
    // T_BOOLEAN = 4
    _type_array_klasses[T_BOOLEAN] = new TypeArrayKlass(T_BOOLEAN, sizeof(jboolean), "[Z");
    // T_CHAR = 5
    _type_array_klasses[T_CHAR]    = new TypeArrayKlass(T_CHAR,    sizeof(jchar),    "[C");
    // T_FLOAT = 6
    _type_array_klasses[T_FLOAT]   = new TypeArrayKlass(T_FLOAT,   sizeof(jfloat),   "[F");
    // T_DOUBLE = 7
    _type_array_klasses[T_DOUBLE]  = new TypeArrayKlass(T_DOUBLE,  sizeof(jdouble),  "[D");
    // T_BYTE = 8
    _type_array_klasses[T_BYTE]    = new TypeArrayKlass(T_BYTE,    sizeof(jbyte),    "[B");
    // T_SHORT = 9
    _type_array_klasses[T_SHORT]   = new TypeArrayKlass(T_SHORT,   sizeof(jshort),   "[S");
    // T_INT = 10
    _type_array_klasses[T_INT]     = new TypeArrayKlass(T_INT,     sizeof(jint),     "[I");
    // T_LONG = 11
    _type_array_klasses[T_LONG]    = new TypeArrayKlass(T_LONG,    sizeof(jlong),    "[J");
}

void TypeArrayKlass::destroy_all() {
    for (int i = 0; i <= T_CONFLICT; i++) {
        if (_type_array_klasses[i] != nullptr) {
            delete _type_array_klasses[i];
            _type_array_klasses[i] = nullptr;
        }
    }
}
