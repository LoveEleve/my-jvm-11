// ============================================================================
// Mini JVM - JavaHeap 实现
// 对应 HotSpot: src/hotspot/share/gc/shared/collectedHeap.cpp
//             + src/hotspot/share/gc/shared/memAllocator.cpp
//
// 核心功能：
//   1. 初始化连续堆内存
//   2. bump-pointer 分配
//   3. 对象分配 = 分配内存 + 清零 + 设置 mark word + 设置 klass 指针
// ============================================================================

#include "gc/shared/javaHeap.hpp"
#include "oops/oop.hpp"
#include "oops/klass.hpp"

#include <cstdlib>
#include <cstring>

// 全局单例
JavaHeap* JavaHeap::_heap = nullptr;

// ============================================================================
// 构造/析构
// ============================================================================

JavaHeap::JavaHeap(size_t capacity_in_bytes) {
    // 对齐到 HeapWord 边界
    _capacity = align_up(capacity_in_bytes, (size_t)HeapWordSize);
    _used = 0;
    _total_allocations = 0;
    _total_allocated_bytes = 0;

    // 分配堆内存（使用 malloc，对应 HotSpot 中 os::reserve_memory + os::commit_memory）
    _base = (HeapWord*)::malloc(_capacity);
    guarantee(_base != nullptr, "JavaHeap: failed to allocate heap memory");

    // 清零整个堆（HotSpot 在 commit 后也会清零）
    memset(_base, 0, _capacity);

    _top = _base;
    _end = (HeapWord*)((char*)_base + _capacity);
}

JavaHeap::~JavaHeap() {
    if (_base != nullptr) {
        ::free(_base);
        _base = nullptr;
        _top = nullptr;
        _end = nullptr;
    }
}

// ============================================================================
// 全局初始化/销毁
// ============================================================================

void JavaHeap::initialize(size_t capacity_in_bytes) {
    guarantee(_heap == nullptr, "JavaHeap already initialized");
    _heap = new JavaHeap(capacity_in_bytes);

    fprintf(stderr, "[JavaHeap] Initialized: capacity=%zu bytes, base=%p, end=%p\n",
            _heap->_capacity, (void*)_heap->_base, (void*)_heap->_end);
}

void JavaHeap::destroy() {
    if (_heap != nullptr) {
        fprintf(stderr, "[JavaHeap] Destroying: used=%zu/%zu bytes, allocations=%d\n",
                _heap->_used, _heap->_capacity, _heap->_total_allocations);
        delete _heap;
        _heap = nullptr;
    }
}

// ============================================================================
// allocate() — 核心分配（bump-the-pointer）
//
// 对应 HotSpot 的分配路径（简化版）：
//   HotSpot: MemAllocator::mem_allocate() → CollectedHeap::allocate_from_tlab_slow()
//            → 最终调到 G1 的 attempt_allocation()
//
// 我们的版本：直接 bump _top 指针
//   - 单线程，不需要 CAS
//   - 不做 TLAB
//   - 堆满返回 nullptr（不触发 GC）
// ============================================================================

HeapWord* JavaHeap::allocate(size_t size_in_words) {
    size_t size_in_bytes = size_in_words * HeapWordSize;

    // 检查是否有足够空间
    HeapWord* new_top = (HeapWord*)((char*)_top + size_in_bytes);
    if (new_top > _end) {
        fprintf(stderr, "[JavaHeap] OOM: requested %zu bytes, free=%zu bytes\n",
                size_in_bytes, free());
        return nullptr;  // OOM
    }

    // Bump the pointer
    HeapWord* result = _top;
    _top = new_top;
    _used += size_in_bytes;

    // 更新统计
    _total_allocations++;
    _total_allocated_bytes += size_in_bytes;

    return result;
}

// ============================================================================
// obj_allocate() — 分配一个 Java 对象实例
//
// 对应 HotSpot 的完整分配 + 初始化链：
//   InterpreterRuntime::_new()
//   → InstanceKlass::allocate_instance()
//     → CollectedHeap::obj_allocate()
//       → MemAllocator::allocate()
//         → mem_allocate()          (分配内存)
//         → finish()                (初始化对象头)
//           → post_allocation_setup_obj()
//             → obj->set_mark(proto)
//             → obj->set_klass(klass)
//
// 我们合并为一个函数：
//   1. 分配 size_in_bytes 的堆内存
//   2. 清零（allocate 分配的区域已经是零，但保险起见再次清零）
//   3. 设置 mark word = prototype (unlocked, age=0, no hash)
//   4. 设置 klass pointer
//   5. 返回 oopDesc*
// ============================================================================

oopDesc* JavaHeap::obj_allocate(Klass* klass, int size_in_bytes) {
    guarantee(klass != nullptr, "obj_allocate: klass must not be null");
    guarantee(size_in_bytes > 0, "obj_allocate: size must be positive");
    guarantee(size_in_bytes >= (int)sizeof(oopDesc), "obj_allocate: size too small for header");

    // 对齐到 HeapWord
    size_t aligned_size = align_up((size_t)size_in_bytes, (size_t)HeapWordSize);
    size_t size_in_words = aligned_size / HeapWordSize;

    // 分配
    HeapWord* mem = allocate(size_in_words);
    if (mem == nullptr) {
        return nullptr;  // OOM
    }

    // 清零（确保所有字段默认值为 0/null）
    // HotSpot: CollectedHeap::init_obj() → Copy::fill_to_aligned_words()
    memset(mem, 0, aligned_size);

    // 转为 oopDesc*
    oopDesc* obj = (oopDesc*)mem;

    // 设置 mark word
    // HotSpot: oopDesc::set_mark(markOopDesc::prototype())
    // prototype = [0...0|001] = unlocked, age=0, no hash, no bias
    obj->set_mark(markOopDesc::prototype());

    // 设置 klass 指针
    // HotSpot: oopDesc::set_klass(klass)
    obj->set_klass(klass);

    return obj;
}

// ============================================================================
// 调试打印
// ============================================================================

void JavaHeap::print_on(FILE* out) const {
    fprintf(out, "=== JavaHeap ===\n");
    fprintf(out, "  Base:     %p\n", (void*)_base);
    fprintf(out, "  Top:      %p\n", (void*)_top);
    fprintf(out, "  End:      %p\n", (void*)_end);
    fprintf(out, "  Capacity: %zu bytes (%.1f MB)\n", _capacity, (double)_capacity / (1024 * 1024));
    fprintf(out, "  Used:     %zu bytes (%.1f%%)\n", _used, _capacity > 0 ? 100.0 * _used / _capacity : 0.0);
    fprintf(out, "  Free:     %zu bytes\n", free());
    fprintf(out, "  Total allocations: %d\n", _total_allocations);
    fprintf(out, "  Total allocated:   %zu bytes\n", _total_allocated_bytes);
}
