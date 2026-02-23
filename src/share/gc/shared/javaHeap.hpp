#ifndef SHARE_GC_SHARED_JAVAHEAP_HPP
#define SHARE_GC_SHARED_JAVAHEAP_HPP

// ============================================================================
// Mini JVM - JavaHeap（简化版 Java 堆）
// 对应 HotSpot: src/hotspot/share/gc/shared/collectedHeap.hpp
//
// HotSpot 的堆是由 GC 算法管理的复杂结构：
//   CollectedHeap (抽象基类)
//   ├── G1CollectedHeap    (G1 GC)
//   ├── CMSHeap            (CMS GC，已废弃)
//   ├── ParallelScavengeHeap (Parallel GC)
//   └── SerialHeap         (Serial GC)
//
// 每种 GC 有不同的堆内存布局和分配策略。
// 但核心分配路径是相同的：
//   1. 先尝试 TLAB 分配（Thread Local Allocation Buffer）
//   2. TLAB 满了 → 慢速路径 → 新 TLAB 或直接堆分配
//   3. 堆满了 → 触发 GC → 回收后重试
//
// 我们的简化版：
//   - 一整块连续内存（类似 Eden 区）
//   - Bump Pointer 分配（top 指针向前推进）
//   - 不做 TLAB（单线程不需要）
//   - 不做 GC（堆满就报 OOM）
//   - 后续 Phase 8-9 实现 G1 GC 时替换
//
// 内存布局：
//   ┌──────────────────────────────────────────┐
//   │ _base                                     │ ← 堆起始地址
//   │ [已分配的对象...]                          │
//   │ _top ──────────────────────────────────── │ ← 下一次分配的位置
//   │ [空闲空间...]                              │
//   │ _end                                      │ ← 堆结束地址
//   └──────────────────────────────────────────┘
//
// 分配过程（bump-the-pointer）：
//   HeapWord* result = _top;
//   _top += size;
//   return result;
// ============================================================================

#include "memory/allocation.hpp"
#include "utilities/globalDefinitions.hpp"

class Klass;
class oopDesc;

class JavaHeap {
private:
    HeapWord* _base;        // 堆起始地址
    HeapWord* _top;         // 当前分配位置（下一个空闲位置）
    HeapWord* _end;         // 堆结束地址
    size_t    _capacity;    // 总容量（字节）
    size_t    _used;        // 已使用（字节）

    // 统计
    int       _total_allocations;  // 总分配次数
    size_t    _total_allocated_bytes;  // 总分配字节数

    // 全局单例
    static JavaHeap* _heap;

public:
    // ======== 构造/析构 ========

    JavaHeap(size_t capacity_in_bytes);
    ~JavaHeap();

    // ======== 全局访问 ========

    static JavaHeap* heap() { return _heap; }
    static void initialize(size_t capacity_in_bytes);
    static void destroy();

    // ======== 核心分配 ========

    // 分配 size_in_words 个 HeapWord
    // 返回起始地址，失败返回 nullptr
    HeapWord* allocate(size_t size_in_words);

    // 分配一个 Java 对象实例
    // 对应 HotSpot: CollectedHeap::obj_allocate()
    // 流程：分配内存 → 清零 → 设置 mark word → 设置 klass 指针
    oopDesc* obj_allocate(Klass* klass, int size_in_bytes);

    // ======== 查询 ========

    size_t capacity()  const { return _capacity; }
    size_t used()      const { return _used; }
    size_t free()      const { return _capacity - _used; }

    HeapWord* base()   const { return _base; }
    HeapWord* top()    const { return _top; }
    HeapWord* end()    const { return _end; }

    // 判断地址是否在堆内
    bool is_in(const void* p) const {
        return p >= (void*)_base && p < (void*)_end;
    }

    // 统计信息
    int    total_allocations()     const { return _total_allocations; }
    size_t total_allocated_bytes() const { return _total_allocated_bytes; }

    // ======== 调试 ========

    void print_on(FILE* out) const;

    NONCOPYABLE(JavaHeap);
};

#endif // SHARE_GC_SHARED_JAVAHEAP_HPP
