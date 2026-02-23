# Phase 2：Class 文件解析器 — ClassFileStream + ConstantPool + ClassFileParser

## 一、为什么 Class 文件解析是第一个"功能性"模块？

JVM 要运行 Java 程序，第一步就是**读取 `.class` 文件**。没有这一步，后面一切都无从谈起：

```
.java 源码  →  javac 编译  →  .class 文件  →  JVM 加载并解析  →  执行
                                                ↑
                                           我们现在要做的
```

HotSpot 中，这个流程的入口是：
1. `ClassLoader` 找到 `.class` 文件并读入内存
2. 构造 `ClassFileStream`（字节流封装）
3. 创建 `ClassFileParser`，调用 `parse_stream()` 解析
4. 解析结果创建为 `InstanceKlass`（类元数据对象）

我们 Phase 2 实现了前 3 步。第 4 步（创建 InstanceKlass）在 Phase 3 实现。

---

## 二、文件清单与对照

| 我们的文件 | HotSpot 对应文件 | 作用 |
|-----------|-----------------|------|
| `src/share/utilities/bytes.hpp` | `utilities/bytes.hpp` + `cpu/x86/bytes_x86.hpp` + `os_cpu/linux_x86/bytes_linux_x86.inline.hpp` | 字节序处理 |
| `src/share/classfile/classFileStream.hpp` | `classfile/classFileStream.hpp` + `.cpp` | 字节流读取器 |
| `src/share/utilities/constantTag.hpp` | `utilities/constantTag.hpp` + `classfile_constants.h.template` | 常量池标签定义 |
| `src/share/oops/constantPool.hpp` | `oops/constantPool.hpp` | 常量池数据结构 |
| `src/share/oops/constantPool.cpp` | `oops/constantPool.cpp` | 常量池实现 |
| `src/share/classfile/classFileParser.hpp` | `classfile/classFileParser.hpp` | 解析器声明 |
| `src/share/classfile/classFileParser.cpp` | `classfile/classFileParser.cpp` (6400+ 行) | 解析器实现 |

---

## 三、逐文件详解

### 3.1 bytes.hpp — 字节序处理

#### 3.1.1 为什么需要字节序转换？

**核心问题：Java `.class` 文件使用大端序，x86 CPU 使用小端序。**

什么是字节序？一个 `u2` 值 `0x0037`（十进制 55）在内存中的存放方式：

```
大端序 (Big-Endian / Network Byte Order / Java Byte Order):
  地址 0: 0x00  ← 高字节在前
  地址 1: 0x37  ← 低字节在后

小端序 (Little-Endian / x86):
  地址 0: 0x37  ← 低字节在前
  地址 1: 0x00  ← 高字节在后
```

如果在 x86 上直接把 `.class` 文件的 2 字节当作 `u2` 读取：

```
文件内容: [0x00, 0x37]
直接读取: *(u2*)p = 0x3700 = 14080  ← 错！
正确应该: 0x0037 = 55              ← 需要翻转
```

#### 3.1.2 HotSpot 的三层架构

HotSpot 的字节序处理分三个文件，体现了它的跨平台设计：

```
bytes.hpp (share/)                 ← 平台无关：Endian 类定义
    ↓ #include
bytes_x86.hpp (cpu/x86/)          ← CPU 相关：模板方法 + swap 声明
    ↓ #include
bytes_linux_x86.inline.hpp        ← OS+CPU 相关：swap 的实际实现
    (os_cpu/linux_x86/)              使用 glibc 的 bswap_16/32/64
```

**为什么要分三层？** 因为 HotSpot 支持多平台：
- SPARC（大端）不需要 swap
- ARM 可能有不同的对齐要求
- Windows x86 的 swap 实现不同于 Linux

**我们的精简版**合并为一个 `bytes.hpp`，因为只支持 Linux x86_64。

#### 3.1.3 数据流

```
.class 文件字节 (大端序)
        │
        ▼
  ClassFileStream.get_u2()
        │
        ▼
  Bytes::get_Java_u2(address)
        │
        ├── get_native<u2>(p)   → memcpy 读取原始字节
        │
        ├── is_Java_byte_ordering_different()?  → x86 上为 true
        │
        └── swap_u2(x)  → bswap_16(x)  → 翻转 2 字节
                │
                ▼
           返回正确的 u2 值 (主机序)
```

> **⚠️ 精简点**
> HotSpot 的 `get_native<T>` 会根据对齐情况选择直接解引用或 `memcpy`：
> ```cpp
> if (is_aligned(p, sizeof(T))) {
>     x = *(T*)p;        // 对齐：直接解引用（更快）
> } else {
>     memcpy(&x, p, sizeof(T));  // 未对齐：安全方式
> }
> ```
> 我们统一用 `memcpy`，因为现代编译器会优化掉这个差异。

---

### 3.2 classFileStream.hpp — 字节流读取器

#### 3.2.1 设计思想

ClassFileStream 封装了"从字节缓冲区顺序读取"的能力。它的核心思想：

```
构造时：调用方已经把 .class 文件完整读入内存

  _buffer_start        _current             _buffer_end
  ↓                    ↓                    ↓
  [CA FE BA BE 00 00 00 37 00 05 ...]
  ├──── 已读取 ───┤├──── 剩余 ────────────┤

每次 get_uN()：
  1. 检查剩余字节够不够
  2. 从 _current 位置读取 N 字节
  3. _current 前移 N 字节
  4. 返回读取的值
```

#### 3.2.2 核心字段（和 HotSpot 完全一致）

```cpp
const u1* const _buffer_start;   // 缓冲区起始（构造后不变）
const u1* const _buffer_end;     // 缓冲区终止（past-the-end）
mutable const u1* _current;      // 当前读取位置
const char* const _source;       // 来源描述（文件路径）
```

**关键设计：`_current` 是 `mutable` 的。**

这是 C++ 的 `mutable` 关键字的经典用法。`get_u2()` 等方法在语义上是"读取"操作（不改变流的内容），所以标记为 `const`。但它们需要移动 `_current` 游标，所以 `_current` 必须是 `mutable` 的。

HotSpot 原始代码也是这样设计的。

#### 3.2.3 两套读取方法

| 方法 | 特点 | 使用场景 |
|------|------|---------|
| `get_u2()` | 带边界检查 | 解析不信任的数据时 |
| `get_u2_fast()` | 无边界检查 | 已经提前验证了剩余长度 |

HotSpot 的性能优化：在 `parse_constant_pool_entries()` 中，先用 `guarantee_more()` 一次性检查整个条目的长度，然后用 `_fast()` 版本逐字段读取，避免每个字段都做边界检查。

```cpp
// HotSpot 风格（性能优化）：
cfs->guarantee_more(5, CHECK);   // 一次性验证 tag(1) + data(4) 字节
u1 tag = cfs->get_u1_fast();    // 无检查
u4 val = cfs->get_u4_fast();    // 无检查

// 我们的精简版：每次都检查（更安全，性能差异可忽略）
u1 tag = cfs->get_u1();         // 带检查
u4 val = cfs->get_u4();         // 带检查
```

> **⚠️ 和 HotSpot 的差异**
> - HotSpot 的 `ClassFileStream` 继承 `ResourceObj`（资源区分配）
> - HotSpot 使用 TRAPS 异常传播机制（`get_u2(TRAPS)` 在错误时抛 Java 异常）
> - 我们直接 `fatal()` + `abort()`，后续添加异常机制后再改

---

### 3.3 constantTag.hpp — 常量池标签

#### 3.3.1 两组标签

常量池标签分两组：

**第一组：JVM 规范标准标签（1-18）**

这些值写死在 `.class` 文件中，由 JVM 规范定义：

```
值  标签名              字节结构                           含义
1   Utf8               u2 length + u1[length]            UTF-8 字符串
3   Integer            u4 bytes                          int 字面量
4   Float              u4 bytes                          float 字面量
5   Long               u4 high + u4 low                  long (占 2 slot)
6   Double             u4 high + u4 low                  double (占 2 slot)
7   Class              u2 name_index                     类/接口引用
8   String             u2 string_index                   字符串字面量
9   Fieldref           u2 class_index + u2 nat_index     字段引用
10  Methodref          u2 class_index + u2 nat_index     方法引用
11  InterfaceMethodref u2 class_index + u2 nat_index     接口方法引用
12  NameAndType        u2 name_index + u2 desc_index     名称和类型
15  MethodHandle       u1 ref_kind + u2 ref_index        方法句柄
16  MethodType         u2 desc_index                     方法类型
17  Dynamic            u2 bsm_index + u2 nat_index       动态常量
18  InvokeDynamic      u2 bsm_index + u2 nat_index       invokedynamic
```

注意：2, 13, 14 是空位（历史原因）。

**第二组：HotSpot 内部标签（100-106）**

这些不会出现在 `.class` 文件中，是 HotSpot 在解析过程中使用的状态标记：

```
值   标签名               含义
0    Invalid             未初始化/Long-Double 占位
100  UnresolvedClass     已读取类名但未实际加载
101  ClassIndex          临时：解析第一遍时 Class 条目存的 name_index
102  StringIndex         临时：解析第一遍时 String 条目存的 utf8_index
103  UnresolvedClassInError  类加载失败的错误状态
```

#### 3.3.2 为什么需要两遍解析？

因为常量池条目之间可以**前向引用**！

```
例如一个 .class 文件的常量池：
  #1 = Class        #5       ← 引用 #5，但 #5 还没读到
  #2 = Methodref    #1.#4    ← 引用 #1 和 #4
  #3 = NameAndType  #6:#7
  #4 = NameAndType  #8:#9
  #5 = Utf8         "HelloWorld"  ← 被 #1 引用
  ...
```

第一遍读取时，遇到 `#1 = Class #5`，此时 `#5` 的 Utf8 还没读到，无法验证。所以：
- **第一遍**：只读取原始数据，Class 条目标记为 `ClassIndex(101)`
- **第二遍**：验证所有交叉引用，`ClassIndex(101)` → `UnresolvedClass(100)`

---

### 3.4 constantPool.hpp — 常量池数据结构

#### 3.4.1 HotSpot 的内联数组设计 vs 我们的独立数组

**HotSpot 的设计**：常量池数据紧跟在 `ConstantPool` 对象之后（内联）：

```
HotSpot 内存布局:
  ┌──────────────────────────────────┐
  │ ConstantPool 头部字段             │  sizeof(ConstantPool)
  │   _tags, _cache, _pool_holder...│
  ├──────────────────────────────────┤
  │ data[0]  (未用)                  │  ← base() 指向这里
  │ data[1]  (第 1 个条目)           │
  │ data[2]  (第 2 个条目)           │
  │ ...                              │
  │ data[n-1]                        │
  └──────────────────────────────────┘

  分配方式：operator new(sizeof(ConstantPool) + length * sizeof(intptr_t))
  访问方式：base()[index]
```

这是 C++ 中"变长对象"的常见技巧。好处是只需一次内存分配，数据和元数据在同一个 cache line 附近。

**我们的设计**：使用独立的数组：

```
我们的内存布局:
  ┌──────────────────┐     ┌──────────────┐     ┌──────────────┐
  │ ConstantPool     │     │ _tags 数组    │     │ _data 数组   │
  │   _tags ──────────────→│ [u1 * length] │     │              │
  │   _data ──────────────────────────────────→  │ [intptr_t *  │
  │   _length        │     └──────────────┘     │  length]     │
  └──────────────────┘                           └──────────────┘
```

> **⚠️ 非最终实现**
> 当前用独立数组是为了简单。后续实现 Metaspace 时，会改为 HotSpot 的内联数组设计。
> 同时还缺少 `_cache`（ConstantPoolCache）、`_pool_holder`（InstanceKlass*）等字段。

#### 3.4.2 数据打包

`Fieldref`、`Methodref` 等条目需要存储两个 `u2` 索引。HotSpot 把它们打包进一个 `jint`：

```
_data[index] = (name_and_type_index << 16) | class_index

  ┌────────────────────────────────────────┐
  │ 高 16 位: name_and_type_index         │ 低 16 位: class_index │
  └────────────────────────────────────────┘

  提取：
    class_index = _data[i] & 0xFFFF
    nat_index   = (_data[i] >> 16) & 0xFFFF
```

这样一个 `intptr_t` 就能存下两个 `u2`，不需要额外的结构体。HotSpot 也是这样做的。

#### 3.4.3 Long/Double 占 2 个 slot

这是 JVM 规范的规定（历史原因，来自 32 位时代）：

```
常量池索引:  1    2    3    4    5    6    ...
            [Int] [Long_____] [Utf8] [Class]
                   ↑    ↑
                   |    index 3 = Invalid (占位)
                   index 2 = Long 值

读取 Long 后必须 index++，跳过占位 slot。
```

在 LP64 下一个 `intptr_t` 就是 8 字节，完全能存下 `jlong`，不需要 2 个 slot。但规范说占 2 个，所以我们必须遵守——否则后面的索引全部偏移。

---

### 3.5 classFileParser.cpp — 解析器核心

#### 3.5.1 整体流程

`parse()` 方法严格按照 JVM 规范 `ClassFile` 结构的顺序：

```
ClassFile {
    u4 magic;                    ← parse_magic_and_version()
    u2 minor_version;            ←
    u2 major_version;            ←
    u2 constant_pool_count;      ← parse_constant_pool()
    cp_info constant_pool[];     ←
    u2 access_flags;             ← parse_access_flags()
    u2 this_class;               ← parse_this_class()
    u2 super_class;              ← parse_super_class()
    u2 interfaces_count;         ← parse_interfaces()
    u2 interfaces[];             ←
    u2 fields_count;             ← parse_fields()
    field_info fields[];         ←
    u2 methods_count;            ← parse_methods()
    method_info methods[];       ←
    u2 attributes_count;         ← parse_class_attributes()
    attribute_info attributes[]; ←
}
```

**这和 HotSpot 的 `parse_stream()` 顺序完全一致**（classFileParser.cpp 第 6071-6316 行）。

#### 3.5.2 常量池解析的 switch-case

`parse_constant_pool_entries()` 中的 switch-case 结构和 HotSpot 完全对应：

```
HotSpot (classFileParser.cpp:127-376)     我们的实现
──────────────────────────────────────     ────────────
case JVM_CONSTANT_Class:                  case JVM_CONSTANT_Class:
  cfs->guarantee_more(3, CHECK);            u2 name_index = cfs->get_u2();
  name_index = cfs->get_u2_fast();          _cp->klass_index_at_put(index, name_index);
  cp->klass_index_at_put(index, name_index);

case JVM_CONSTANT_Utf8:                   case JVM_CONSTANT_Utf8:
  cfs->guarantee_more(2, CHECK);            u2 utf8_length = cfs->get_u2();
  utf8_length = cfs->get_u2_fast();         const u1* utf8_buffer = cfs->get_u1_buffer();
  ... (批量 Symbol 分配)                     cfs->skip_u1(utf8_length);
                                             _cp->utf8_at_put(index, utf8_buffer, utf8_length);
```

> **⚠️ 和 HotSpot 的关键差异**
>
> **1. Utf8 的处理方式不同**
> - HotSpot：创建 `Symbol` 对象，存入 `SymbolTable`（全局去重）
> - 我们：直接 `malloc` 拷贝 char*（不去重）
> - 原因：`Symbol` 和 `SymbolTable` 是复杂的基础设施，后续 Phase 实现
>
> **2. 没有 TRAPS 异常传播**
> - HotSpot：解析错误通过 TRAPS 机制抛出 `ClassFormatError`
> - 我们：直接 `fatal()` + `abort()`
>
> **3. 没有验证 (verify)**
> - HotSpot：根据 `_need_verify` 做详细的格式验证
> - 我们：只做最基本的边界检查
>
> **4. FieldInfo / MethodInfo 是临时结构**
> - HotSpot 使用 `Array<u2>*` 和 `Method*` 等 Metaspace 对象
> - 我们用简单的 struct，后续 Phase 3 会改为 HotSpot 风格

#### 3.5.3 Code 属性解析

```
Code_attribute {
    u2 max_stack;           ← 操作数栈最大深度
    u2 max_locals;          ← 局部变量表大小
    u4 code_length;         ← 字节码长度
    u1 code[code_length];   ← 字节码本体（我们拷贝到堆上）
    u2 exception_table_length;
    exception_table[...];   ← 异常表（暂时跳过）
    u2 attributes_count;
    attributes[...];        ← LineNumberTable 等（暂时跳过）
}
```

字节码是方法的核心——`code[]` 数组就是后续解释器要执行的指令序列。我们将它完整拷贝到堆上，为 Phase 5（字节码解释器）做准备。

---

## 四、验证结果

### 4.1 我们的输出

```
=== Class File Summary ===
  Version: 55.0 (Java 11)
  Access:  0x0021 public super
  Class:   HelloWorld
  Super:   java/lang/Object
  Fields: 2
    [0] I count (flags=0x0002)
    [1] Ljava/lang/String; MESSAGE (flags=0x0019)
  Methods: 3
    [0] <init>()V (flags=0x0001, max_stack=2, max_locals=1, code_length=10)
    [1] add(II)I (flags=0x0001, max_stack=2, max_locals=3, code_length=4)
    [2] main([Ljava/lang/String;)V (flags=0x0009, max_stack=3, max_locals=3, code_length=23)
```

### 4.2 javap 的输出（对照）

```
  private int count;                           ← 对应 [0] I count (flags=0x0002)
    flags: (0x0002) ACC_PRIVATE

  public static final String MESSAGE;          ← 对应 [1] Ljava/lang/String; MESSAGE (flags=0x0019)
    flags: (0x0019) ACC_PUBLIC, ACC_STATIC, ACC_FINAL

  public HelloWorld();                         ← 对应 [0] <init>()V
    Code: stack=2, locals=1                    ← max_stack=2, max_locals=1 ✓

  public int add(int, int);                    ← 对应 [1] add(II)I
    Code: stack=2, locals=3                    ← max_stack=2, max_locals=3 ✓

  public static void main(String[]);           ← 对应 [2] main([Ljava/lang/String;)V
    Code: stack=3, locals=3                    ← max_stack=3, max_locals=3 ✓
```

### 4.3 常量池对比

```
我们的输出:                              javap 的输出:
  #14 = String  #34  // Hello, Mini JVM!    #14 = String  #34  // Hello, Mini JVM!  ✓
  #25 = NameAndType #15:#16 // <init>:()V   #25 = NameAndType #15:#16 // "<init>":()V ✓
  #29 = UnresolvedClass #35 // java/lang/System  #29 = Class #35 // java/lang/System ✓
```

唯一的差异是 `Class` vs `UnresolvedClass`——这是因为 HotSpot 最终会解析为 `Klass*`，显示为 `Class`；我们还没做解析，所以显示 `UnresolvedClass`。本质上数据完全一致。

---

## 五、当前实现 vs HotSpot 差异总结

| 组件 | 状态 | 差异说明 |
|------|------|---------|
| `bytes.hpp` 字节序处理 | ✅ 最终实现 | 合并了 3 个文件为 1 个，功能完整 |
| `ClassFileStream` 字节流 | ⚠️ 精简版 | 缺少 TRAPS 异常传播，不继承 ResourceObj |
| `constantTag` 标签定义 | ✅ 最终实现 | 标签值和 HotSpot 完全一致 |
| `ConstantPool` 常量池 | ⚠️ 精简版 | 用独立数组而非内联数组；Utf8 用 char* 而非 Symbol；缺少 _cache/_pool_holder |
| `ClassFileParser` 解析器 | ⚠️ 精简版 | 核心流程完整，但缺少：验证、注解解析、字段布局计算、vtable 大小计算 |
| `FieldInfo` 字段信息 | ⚠️ 临时结构 | 后续改为 HotSpot 的 `Array<u2>*` 打包格式 |
| `MethodInfo` 方法信息 | ⚠️ 临时结构 | 后续改为 HotSpot 的 `Method*`（Metaspace 对象）|
| Code 属性解析 | ⚠️ 精简版 | 字节码已拷贝，但异常表和 LineNumberTable 暂时跳过 |
| 类属性解析 | ⚠️ 精简版 | SourceFile, InnerClasses 等暂时跳过 |

---

## 六、下一步：Phase 3

Phase 3 将实现 **对象模型（OOP-Klass）**：

```
需要实现的核心类：
  src/share/oops/oopDesc.hpp        ← Java 对象的内存布局
  src/share/oops/markOop.hpp        ← 对象头 mark word
  src/share/oops/klass.hpp          ← 类元数据基类
  src/share/oops/instanceKlass.hpp  ← 普通 Java 类的元数据

目标：
  ClassFileParser 解析出的信息 → 创建 InstanceKlass 对象
  InstanceKlass 保存：常量池、方法数组、字段布局、vtable 等
```

这一步完成后，我们的 JVM 就有了完整的"类表示"——知道一个类有哪些方法、哪些字段、父类是谁。为 Phase 4（线程和栈帧）和 Phase 5（字节码执行）打下基础。
