# Phase 1：基础设施 — 类型系统、断言、内存分配

## 一、为什么第一步要写这些？

HotSpot 的代码不是"普通的 C++ 程序"。它有一套自己的**基础设施层**，几乎所有后续代码都建立在这个基础设施之上。如果不先把这层搞好，后面每一步都会卡住。

具体来说：

```
后续每一个文件，都会：
  1. 使用 jint/jlong/u1/u2 等类型      → 需要 globalDefinitions.hpp
  2. 使用 vm_assert/guarantee 做检查    → 需要 debug.hpp
  3. 继承 CHeapObj<F> 来分配内存        → 需要 allocation.hpp
  4. 引用 oop/Klass 等类型              → 需要 oopsHierarchy.hpp
```

所以这些是**地基**，必须先建。

---

## 二、文件清单与对照

| 我们的文件 | HotSpot 对应文件 | 作用 |
|-----------|-----------------|------|
| `src/share/utilities/globalDefinitions.hpp` | `hotspot/share/utilities/globalDefinitions.hpp` | 基础类型、常量、工具函数 |
| `src/share/utilities/debug.hpp` | `hotspot/share/utilities/debug.hpp` | 断言和错误报告 |
| `src/share/utilities/macros.hpp` | `hotspot/share/utilities/macros.hpp` | 条件编译宏 |
| `src/share/memory/allocation.hpp` | `hotspot/share/memory/allocation.hpp` | 内存分配基类 |
| `src/share/memory/allocation.cpp` | `hotspot/share/memory/allocation.cpp` | 内存分配实现（当前为空壳） |
| `src/share/oops/oopsHierarchy.hpp` | `hotspot/share/oops/oopsHierarchy.hpp` | OOP/Klass 类型层次 |

---

## 三、逐文件详解

### 3.1 globalDefinitions.hpp — 类型系统

#### 3.1.1 为什么不直接用 `int`/`long`？

C++ 的 `int` 和 `long` 在不同平台上大小不同：

```
             ILP32 (32位)    LP64 (Linux 64位)    LLP64 (Windows 64位)
  int        32 bit          32 bit               32 bit
  long       32 bit          64 bit               32 bit     ← 不一致！
  long long  64 bit          64 bit               64 bit
  指针       32 bit          64 bit               64 bit
```

HotSpot 需要跨平台运行，所以定义了自己的类型：

```cpp
typedef int  jint;    // 始终 32 位
typedef long jlong;   // LP64 下 = 64 位（Linux/Mac）
```

**我们的精简版**只支持 Linux x86_64 (LP64)，所以 `long = 64 bit` 是确定的。但我们仍然保留这些 typedef，因为：
1. 后续代码中到处使用 `jint`/`jlong`，保持一致
2. 和 HotSpot 源码对照时能一一对应

#### 3.1.2 u1/u2/u4/u8 是什么？

这是 **JVM 规范**定义的类型，专门用于 Class 文件格式：

```
Java Class 文件就是一个字节流，JVM 规范中这样描述每个字段的大小：
  magic:         u4    → 0xCAFEBABE
  minor_version: u2
  major_version: u2
  constant_pool_count: u2
  ...
```

所以 `u1/u2/u4/u8` 不是我们发明的，是 JVM 规范的标准命名。后续写 ClassFileParser 时会大量使用。

```cpp
typedef jubyte  u1;    // unsigned 1 字节 — 用于读 class 文件的单字节
typedef jushort u2;    // unsigned 2 字节 — 用于读 class 文件的双字节
typedef juint   u4;    // unsigned 4 字节
typedef julong  u8;    // unsigned 8 字节
```

#### 3.1.3 address 类型

```cpp
typedef u_char* address;   // 通用字节地址
```

这是 HotSpot 中**最核心的指针类型**之一。为什么不直接用 `void*` 或 `char*`？

- `void*` 不能做指针算术（`void* p; p + 1` 是非法的）
- `char*` 语义不清（是字符串？还是地址？）
- `address` = `unsigned char*`，明确表示"这是一个字节地址"，可以做指针运算

在后续代码中，几乎所有涉及"内存地址"的地方都用 `address`：
- 对象在堆中的地址
- 方法入口地址
- JIT 编译代码的地址
- 栈帧中的地址

#### 3.1.4 HeapWord 类型

```cpp
class HeapWord {
    char* i;   // 不透明——你不能直接访问
};
```

**这是 HotSpot 最精妙的设计之一。** 为什么要搞一个这样的"空壳类"？

**问题**：Java 堆中，所有对象都是 8 字节对齐的。当我们想表示"堆中的一个位置"时，单位应该是什么？

- 如果用 `char*`（字节级），那 `p + 1` 只移动 1 字节，容易出错
- 如果用 `HeapWord*`（字级），那 `p + 1` 移动 `sizeof(HeapWord) = 8` 字节，刚好是一个对齐单位

```
HeapWord* p = ...;
p + 1;    // 移动 8 字节（一个 HeapWord）
p + n;    // 移动 n * 8 字节

这样做指针运算时，单位自然就是 "字"（word），而不是字节，
大大减少了出错的可能。
```

> **⚠️ 注意：当前为精简版**  
> HotSpot 的 HeapWord 还配合了大量的 `pointer_delta`、`byte_offset` 等工具函数。我们后续在用到时再补充。

#### 3.1.5 BasicType 枚举

```cpp
enum BasicType {
    T_BOOLEAN =  4,
    T_CHAR    =  5,
    ...
    T_OBJECT  = 12,
    T_ARRAY   = 13,
    ...
};
```

**为什么编号从 4 开始？**

这是 JVM 规范定义的，和字节码指令中的 `atype` 操作数对应。比如 `newarray` 指令创建基本类型数组时，操作数就是：

```
newarray 4   → boolean[]
newarray 5   → char[]
newarray 6   → float[]
newarray 7   → double[]
newarray 8   → byte[]
newarray 9   → short[]
newarray 10  → int[]
newarray 11  → long[]
```

0-3 被跳过了，因为历史原因（早期 JVM 实现保留了这些编号）。HotSpot 沿用了这个编码，所以我们也保持一致。

#### 3.1.6 对齐工具函数

```cpp
inline uintx align_up(uintx size, uintx alignment) {
    uintx mask = alignment - 1;
    return (size + mask) & ~mask;
}
```

这是 JVM 中**使用频率最高的工具函数之一**，几乎到处都在用：

```
场景 1：对象分配
  对象大小必须对齐到 8 字节 → align_up(real_size, 8)

场景 2：Region 大小
  G1 的 Region 必须对齐到 page 边界 → align_up(region_size, page_size)

场景 3：堆空间
  堆的起始/结束地址必须对齐 → align_up(heap_start, alignment)
```

原理（以 `align_up(7, 8)` 为例）：

```
alignment = 8 = 0b1000
mask = 7      = 0b0111

size = 7      = 0b0111
size + mask   = 0b1110  (14)
~mask         = 0b...11111000
(size+mask) & ~mask = 0b1000 = 8  ✓

直觉：先加上 (alignment-1) 确保"够了"，再清除低位"截断"到对齐边界。
```

---

### 3.2 debug.hpp — 断言与错误报告

#### 3.2.1 为什么需要自己的断言系统？

C 标准库有 `assert()`，但 HotSpot 不用它，原因：

1. **需要自定义错误信息**：`assert(p)` 只打印表达式，不能打印上下文
2. **需要区分严重等级**：`assert`（开发期）vs `guarantee`（始终生效）
3. **HotSpot 需要生成 `hs_err_pid.log`**（我们精简版不做这个）

```
HotSpot 的断言体系：

  assert(condition, "message")    — 仅 Debug 构建生效
                                    失败 → 报错 + BREAKPOINT
                                    Release 构建中完全移除（零开销）

  guarantee(condition, "message") — 始终生效
                                    用于关键的运行时检查
                                    即使 Release 构建也会检查

  fatal("message")                — 无条件报错
                                    用于"绝对不应该到达这里"的情况
```

#### 3.2.2 vm_assert vs assert

我们把 HotSpot 的 `assert` 改名为 `vm_assert`，是因为 C 标准库已经有 `assert` 宏了，避免冲突。

```cpp
// 我们的：
#ifdef ASSERT
#define vm_assert(p, msg)  do { if (!(p)) { report_vm_error(...); } } while(0)
#else
#define vm_assert(p, msg)  do {} while(0)   // Release 构建：完全消失
#endif
```

> **⚠️ 和 HotSpot 的差异**  
> HotSpot 的 `assert` 支持 `printf` 格式化：`assert(x > 0, "x = %d", x)`  
> 我们的精简版只接受字符串，不支持格式化参数。后续可以补充。

#### 3.2.3 ShouldNotReachHere / ShouldNotCallThis

```cpp
#define ShouldNotReachHere()  report_vm_error(__FILE__, __LINE__, "ShouldNotReachHere()")
#define ShouldNotCallThis()   report_vm_error(__FILE__, __LINE__, "ShouldNotCallThis()")
```

这两个宏在 HotSpot 中大量使用：

```cpp
// ShouldNotReachHere —— 用在 switch 的 default 分支
switch (type) {
    case T_INT:    ... break;
    case T_LONG:   ... break;
    default: ShouldNotReachHere();  // 如果走到这里，说明有 bug
}

// ShouldNotCallThis —— 用在不应该被调用的方法
class AllStatic {
    AllStatic() { ShouldNotCallThis(); }  // 静态类不允许创建实例
};
```

---

### 3.3 allocation.hpp — 内存分配基类

#### 3.3.1 HotSpot 的内存分配哲学

HotSpot 中每个 C++ 对象的内存都有明确的**来源**和**生命周期**。通过继承不同的基类，强制规范内存管理：

```
┌─────────────────────────────────────────────────────────────────────┐
│                    HotSpot 内存分配体系                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  CHeapObj<F>     malloc/free          手动管理，最常用               │
│  │               ↑                    几乎所有 VM 内部对象           │
│  │               用 new/delete                                      │
│  │                                                                  │
│  StackObj        栈上                 RAII 类（MutexLocker 等）      │
│  │               ↑                    禁止 new，出作用域自动销毁     │
│  │               直接声明局部变量                                    │
│  │                                                                  │
│  ResourceObj     ResourceArea         临时对象（编译期间等）          │
│  │               ↑                    批量释放，无需逐个 delete       │
│  │               线程本地的内存池                                    │
│  │                                                                  │
│  MetaspaceObj    Metaspace            类元数据（Klass, Method 等）   │
│  │               ↑                    随类卸载一起释放               │
│  │               专用的非堆内存                                      │
│  │                                                                  │
│  AllStatic       无实例               纯静态类（Universe 等）        │
│                  所有成员都是 static                                 │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

#### 3.3.2 CHeapObj 详解

```cpp
template <MEMFLAGS F>
class CHeapObj {
public:
    void* operator new(size_t size) throw() {
        return AllocateHeap(size, F);   // → malloc()
    }
    void operator delete(void* p) {
        FreeHeap(p);                     // → free()
    }
};
```

**为什么用模板参数 `F`？**

`F` 是 `MEMFLAGS`（即 `MemoryType`），标识这块内存的用途。HotSpot 有一个叫 **NMT (Native Memory Tracking)** 的功能，可以统计 VM 各部分使用了多少内存：

```
$ java -XX:NativeMemoryTracking=summary -XX:+UnlockDiagnosticVMOptions -XX:+PrintNMTStatistics ...

Native Memory Tracking:
  Java Heap:     8192 MB    ← mtJavaHeap
  Class:          128 MB    ← mtClass
  Thread:          64 MB    ← mtThread
  Code:            48 MB    ← mtCode
  GC:              32 MB    ← mtGC
  ...
```

每个 `CHeapObj<mtXxx>` 的子类，在 `new` 的时候会告诉 NMT "这块内存属于 mtXxx 类别"。这样就能精确统计每个子系统用了多少内存。

**我们的精简版**不实现 NMT，但保留了 `MEMFLAGS` 参数，因为：
1. 后续代码中所有继承 `CHeapObj` 的类都会带这个参数
2. 保持和 HotSpot 一致的写法

> **⚠️ 非最终实现**  
> 当前 `AllocateHeap` 直接调 `malloc`。HotSpot 版本会：
> 1. 经过 NMT 记录分配信息
> 2. 在 Debug 模式下填充 "坏" 值（如 0xAB）帮助检测未初始化
> 3. 检查内存分配对齐
>
> 后续可以按需补充这些功能。

#### 3.3.3 StackObj 详解

```cpp
class StackObj {
private:
    void* operator new(size_t size) throw();     // private！禁止 new
    void  operator delete(void* p);              // private！禁止 delete
};
```

把 `new`/`delete` 设为 `private` 后：

```cpp
class MutexLocker : public StackObj {
    Mutex* _mutex;
public:
    MutexLocker(Mutex* m) : _mutex(m) { m->lock(); }
    ~MutexLocker() { _mutex->unlock(); }
};

// 正确用法：
{
    MutexLocker ml(&some_mutex);  // 栈上创建，自动析构
    // ... 受保护的代码 ...
}  // ml 析构，自动解锁

// 错误用法：
MutexLocker* ml = new MutexLocker(&some_mutex);  // 编译错误！new 是 private
```

这就是 C++ 的 RAII 模式。通过继承 `StackObj`，**编译期就阻止**了你犯"忘记 delete"的错误。

#### 3.3.4 AllStatic 详解

```cpp
class AllStatic {
    AllStatic()  { ShouldNotCallThis(); }
    ~AllStatic() { ShouldNotCallThis(); }
};
```

这是给"纯静态类"用的。HotSpot 中有很多类，它们只是把一堆相关的 static 函数/变量分组到一起：

```cpp
// Universe — 全局状态
class Universe : AllStatic {
    static oop _main_thread_group;
    static Klass* _objectArrayKlassObj;
    // ... 全是 static
public:
    static void initialize();
    static oop main_thread_group() { return _main_thread_group; }
};

// os — 操作系统接口
class os : AllStatic {
public:
    static void* malloc(size_t size);
    static void  free(void* p);
    static int   vm_page_size();
};
```

本质上 `AllStatic` 就是 C++ 11 之前的 "namespace + 不让你实例化" 的技巧。

> **⚠️ 非最终实现**  
> 我们还没实现 `ResourceObj` 和 `MetaspaceObj`：
> - **ResourceObj** 将在实现解释器时添加（编译期间的临时对象大量使用它）
> - **MetaspaceObj** 将在实现类加载时添加（Klass、Method 等元数据继承它）

---

### 3.4 oopsHierarchy.hpp — 对象指针类型

#### 3.4.1 OOP-Klass 二分模型

HotSpot 的对象模型有一个核心设计：**把 Java 对象分成两部分存储**。

```
  Java 层面：
    Object obj = new MyClass();

  JVM 内部：分成两块

  ┌──────────────────────────┐    ┌──────────────────────────────┐
  │     instanceOopDesc      │    │       InstanceKlass          │
  │     (Java 堆中)           │    │       (Metaspace 中)         │
  ├──────────────────────────┤    ├──────────────────────────────┤
  │ markOop  _mark           │    │ 类名、父类、接口列表          │
  │ Klass*   _metadata ──────────→│ 方法表 (vtable)              │
  ├──────────────────────────┤    │ 字段布局信息                  │
  │ field_1: int = 42        │    │ 常量池指针                    │
  │ field_2: Object = ...    │    │ ...                          │
  │ ...                      │    └──────────────────────────────┘
  └──────────────────────────┘
      ↑                               ↑
      oop (实例数据)                    Klass (类元数据)
      每个对象一份                       同类所有对象共享一份
```

**为什么要分开？**

1. **节省内存**：方法表、字段描述等信息所有实例共享，不需要每个对象都存一份
2. **GC 效率**：oop 在堆中可以被 GC 移动，Klass 在 Metaspace 中不参与 GC
3. **对照 Java 语义**：`obj.getClass()` 返回的就是 Klass 对应的 mirror 对象

#### 3.4.2 oop 类型 = 就是一个指针的 typedef

```cpp
typedef oopDesc*          oop;          // 所有对象的基类指针
typedef instanceOopDesc*  instanceOop;  // Java 实例
typedef arrayOopDesc*     arrayOop;     // 数组基类
typedef objArrayOopDesc*  objArrayOop;  // Object[]
typedef typeArrayOopDesc* typeArrayOop; // int[], byte[] 等
```

**为什么要 typedef？直接用 `oopDesc*` 不行吗？**

1. **简洁**：到处写 `oopDesc*` 太长，`oop` 更清爽
2. **语义明确**：看到 `oop` 就知道"这是一个 Java 对象指针"
3. **统一抽象**：后续可以在不改业务代码的情况下，改变 oop 的底层表示

> **⚠️ HotSpot 在 Debug 模式下的差异**  
> HotSpot 定义了 `CHECK_UNHANDLED_OOPS` 宏，在 Debug 模式下 `oop` 不是简单 typedef，
> 而是一个**类**，包裹了 `oopDesc*` 指针。这样可以在编译期检测"裸 oop 没有通过 Handle 保护"的 bug。
>
> 我们精简版用简单 typedef，后续如果需要可以升级。

#### 3.4.3 narrowOop — 压缩对象指针

```cpp
typedef juint narrowOop;    // 32 位
```

这是 HotSpot 的重要优化。在 64 位 JVM 中，每个 oop 指针占 8 字节。如果堆中有大量对象互相引用，指针本身就占很多空间。

**压缩指针原理**：

```
  条件：堆大小 ≤ 32GB，对象 8 字节对齐

  实际地址 = base + (narrowOop << 3)

  narrowOop 只有 32 位（4 字节），但因为左移 3 位，
  可以寻址 2^32 * 8 = 32GB 的堆空间。

  节省：每个引用从 8 字节 → 4 字节，节省 50% 指针空间。
```

这在对象头和字段引用中大量使用，是 HotSpot 的核心优化之一。

> **⚠️ 非最终实现**  
> 当前只定义了类型。压缩/解压缩的具体逻辑（`oopDesc::decode_heap_oop` 等）
> 会在实现对象模型时补充。

#### 3.4.4 Klass 层次的前向声明

```cpp
class Klass;
class InstanceKlass;
class ArrayKlass;
class ObjArrayKlass;
class TypeArrayKlass;
// ...
```

这些 class 只是**前向声明**，还没有实际定义。它们会在后续 Phase 中逐步实现：

| 类 | 用途 | 实现时机 |
|---|---|---|
| `Klass` | 所有类元数据的基类 | Phase 3 (对象模型) |
| `InstanceKlass` | 普通 Java 类的元数据 | Phase 3 |
| `ArrayKlass` | 数组类的基类 | Phase 7 (对象创建) |
| `Method` | 方法元数据 | Phase 3 |
| `ConstantPool` | 常量池 | Phase 2 (Class 解析) |

---

### 3.5 macros.hpp — 条件编译

```cpp
#define DEBUG_ONLY(code)  code      // Debug 模式下展开
#define G1GC_ONLY(code)   code      // G1 GC 模式下展开
#define LP64_ONLY(code)   code      // 64 位模式下展开
```

HotSpot 支持多种 GC（Serial, Parallel, G1, ZGC, Shenandoah）、多种平台（Linux, Windows, Mac）、多种 CPU（x86, aarch64, arm）。通过这些宏在编译时选择代码。

**我们只支持 Linux x86_64 + G1**，所以这些宏大部分是固定展开的。保留它们是为了和 HotSpot 代码对照时能看出"这段代码在哪些条件下才存在"。

---

## 四、当前实现 vs HotSpot 的差异总结

| 组件 | 我们的实现 | HotSpot 实现 | 差异原因 |
|------|-----------|-------------|---------|
| **类型定义** | 单文件 | 多文件链式 include (`jni_md.h` → `jni.h` → `globalDefinitions.hpp` → 平台特定头文件) | 我们只支持一个平台，不需要分层 |
| **assert** | 打印 + `abort()` | 生成 `hs_err_pid.log` + 核心转储 | 精简版不需要复杂错误报告 |
| **CHeapObj** | 直接 `malloc`/`free` | 经过 NMT 追踪 | 精简版不实现 NMT |
| **ResourceObj** | ❌ 未实现 | Arena + ResourceArea | 后续需要时添加 |
| **MetaspaceObj** | ❌ 未实现 | Metaspace 分配器 | Phase 3 实现 |
| **oop** | 简单 typedef | Debug 模式下是类（CHECK_UNHANDLED_OOPS） | 精简版不需要这个检查 |

> **标记说明**：
> - ✅ = 最终实现（和 HotSpot 设计一致）
> - ⚠️ = 精简版（核心设计一致，但省略了部分功能）
> - ❌ = 未实现（后续 Phase 补充）

| 文件 | 状态 |
|------|------|
| `globalDefinitions.hpp` | ⚠️ 精简版 — 类型定义是最终的，工具函数后续会补充 |
| `debug.hpp` | ⚠️ 精简版 — 断言逻辑是最终的，错误报告后续可增强 |
| `allocation.hpp` | ⚠️ 精简版 — `CHeapObj`/`StackObj`/`AllStatic` 是最终的，`ResourceObj`/`MetaspaceObj` 后续添加 |
| `oopsHierarchy.hpp` | ⚠️ 精简版 — 类型声明是最终的，具体类定义后续实现 |
| `macros.hpp` | ✅ 最终实现 |

---

## 五、测试验证

运行 `build/mini_jvm` 的输出：

```
========================================
  Mini JVM - Phase 1: Foundation Test
========================================

=== Test: Basic Types ===
  jbyte    : 1 bytes         ✓ 和 HotSpot 一致
  jint     : 4 bytes         ✓
  jlong    : 8 bytes         ✓
  address  : 8 bytes         ✓ LP64 下指针 = 8 字节
  HeapWord : 8 bytes         ✓ sizeof(HeapWord) = sizeof(char*) = 8
  oopSize  : 8 bytes         ✓
  BytesPerWord = 8           ✓
  BitsPerWord  = 64          ✓

=== Test: Alignment ===
  align_up(7, 8)    = 8     ✓
  align_up(9, 8)    = 16    ✓
  align_down(15, 8) = 8     ✓

=== Test: CHeapObj ===
  TestObject allocated       ✓ malloc 正常工作
  arr[5] = 25               ✓ 数组分配正常

=== Test: BasicType ===
  T_INT    size = 4 bytes   ✓ 和 JVM 规范一致
  T_LONG   size = 8 bytes   ✓
  T_OBJECT size = 8 bytes   ✓ LP64 下对象引用 = 8 字节
```

---

## 六、下一步：Phase 2

Phase 2 将实现 **Class 文件解析器**（`ClassFileParser`），这是 JVM 的入口——要运行 Java 程序，第一步就是把 `.class` 文件读进来，解析出类名、方法、字段、常量池等信息。

需要用到本次实现的：
- `u1/u2/u4` 类型 → 读取 class 文件的字节流
- `CHeapObj<mtClass>` → 分配解析结果的存储空间
- `vm_assert/guarantee` → 验证 class 文件格式合法性

```
Phase 2 要实现的文件：
  src/share/classfile/classFileParser.hpp   ← 对应 HotSpot classFileParser.hpp
  src/share/classfile/classFileParser.cpp
  src/share/classfile/classFileStream.hpp   ← 字节流读取器
  src/share/classfile/classFileStream.cpp
  src/share/oops/constantPool.hpp           ← 常量池数据结构
  src/share/oops/constantPool.cpp
```
