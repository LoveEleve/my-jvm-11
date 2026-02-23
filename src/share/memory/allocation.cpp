// ============================================================================
// Mini JVM - 内存分配实现
// 对应 HotSpot: src/hotspot/share/memory/allocation.cpp
//
// HotSpot 版本有复杂的 NMT 追踪、ResourceObj 分配类型编码等。
// 当前精简版的分配逻辑都在 allocation.hpp 中 inline 实现了，
// 这个文件预留给后续扩展（Arena、ResourceObj 等）。
// ============================================================================

#include "memory/allocation.hpp"

// 后续会在这里实现：
// - Arena::Amalloc()    — Arena 分配器
// - ResourceObj 的分配类型追踪
// - MetaspaceObj 的分配
