# Phase 3：OOP-Klass 对象模型

## 一、为什么 OOP-Klass 是 JVM 最核心的设计？

Java 说"万物皆对象"，但 JVM 内部需要回答一个根本问题：**Java 对象在内存中到底长什么样？**

HotSpot 的答案是 **OOP-Klass 二元结构**：

```
Java 对象（堆中）              类元数据（元空间）
┌─────────────────┐           ┌──────────────────────┐
│ oopDesc          │           │ InstanceKlass         │
│  _mark (8B)      │           │  _layout_helper       │
│  _klass ─────────┼──────────→│  _constants           │
│  [field data]    │           │  _methods             │
└─────────────────┘           │  _field_infos         │
                              │  _access_flags        │
                              │  _vtable_len          │
                              └──────────────────────┘
```

核心设计原则：
1. **oopDesc 没有 C++ 虚表** → 对象头只有 16 字节（而不是 24 字节）
2. **所有虚行为通过 `_klass` 指针转发** → 节省堆空间，堆中对象数量远大于类数量
3. **Klass 继承 Metadata，有 C++ 虚表** → 可以多态派发

---

## 二、文件清单与对照

| 我们的文件 | HotSpot 对应文件 | 作用 |
|-----------|-----------------|------|
| `src/share/oops/markOop.hpp` | `oops/markOop.hpp` | Mark Word 位域编解码 |
| `src/share/oops/oop.hpp` | `oops/oop.hpp` | Java 对象基类 oopDesc |
| `src/share/oops/instanceOop.hpp` | `oops/instanceOop.hpp` | 普通对象实例 |
| `src/share/oops/metadata.hpp` | `oops/metadata.hpp` | 类元数据基类 |
| `src/share/oops/klass.hpp` | `oops/klass.hpp` | 类元数据 Klass |
| `src/share/oops/instanceKlass.hpp` | `oops/instanceKlass.hpp` | Java 类元数据 |
| `src/share/oops/instanceKlass.cpp` | `oops/instanceKlass.cpp` | InstanceKlass 实现 |
| `src/share/oops/method.hpp` | `oops/method.hpp` | 方法元数据 |
| `src/share/oops/constMethod.hpp` | `oops/constMethod.hpp` | 方法不可变部分 |
| `src/share/utilities/accessFlags.hpp` | `utilities/accessFlags.hpp` | 访问标志 |

修改的已有文件：
| 文件 | 修改内容 |
|------|---------|
| `src/share/oops/oopsHierarchy.hpp` | 更新 markOop 注释 |
| `src/share/oops/constantPool.hpp` | 添加 `_pool_holder` 字段（回指 InstanceKlass） |
| `src/share/classfile/classFileParser.hpp` | 删除内联 AccessFlags enum，添加 `create_instance_klass()` |
| `src/share/classfile/classFileParser.cpp` | 实现 `create_instance_klass()` |
| `src/share/utilities/globalDefinitions.hpp` | 添加 `BitsPerLong`，`nth_bit`/`right_n_bits` 改为 constexpr |

---

## 三、逐文件详解

### 3.1 markOop.hpp — 对象头 Mark Word

#### 3.1.1 核心概念

Mark Word 是对象头的第一个字（64 位），编码了对象的运行时状态。

**关键理解：markOop 不是真正的对象！**

`markOopDesc` 在 HotSpot 中继承自 `oopDesc`，但它本身就是一个被当作位域使用的指针值。`(uintptr_t)this` 就是 mark word 的值。永远不会 `new markOopDesc()`。`markOop = markOopDesc*` 只是为了利用 C++ 方法调用语法。

我们的精简版独立定义 `markOopDesc`（不继承 `oopDesc`），避免循环依赖。

#### 3.1.2 64 位 Mark Word 布局

```
 63                 39 38      8  7   6 5 4   3  2   0
┌─────────────────────┬────────┬──┬───┬─────┬───┬────┐
│     unused (25)     │hash(31)│ 1│cms│age(4)│ B │lock│
└─────────────────────┴────────┴──┴───┴─────┴───┴────┘
                                                      ↑
                                                     LSB
```

| 字段 | 位宽 | 移位量 | 掩码 | 说明 |
|------|------|--------|------|------|
| lock | 2 | 0 | 0x3 | 锁状态 |
| biased_lock (B) | 1 | 2 | - | 偏向锁标志 |
| age | 4 | 3 | 0xF（在位置0x78） | GC 分代年龄 |
| cms | 1 | 7 | - | CMS 标志位 |
| hash | 31 | 8 | 0x7FFFFFFF | identity hash code |
| unused | 25 | - | - | 未使用 |

Lock 状态编码（最低 2-3 位）：

| lock 值 | 含义 | 高位内容 |
|---------|------|---------|
| 00 | 轻量级锁（thin lock） | 指向栈上 displaced header |
| 01（biased=0） | 无锁（unlocked） | hash + age |
| 10 | 重量级锁（inflated） | 指向 ObjectMonitor |
| 11 | GC 标记（marked） | 仅 GC 时使用 |
| 101（biased=1） | 偏向锁 | JavaThread* + epoch + age |

#### 3.1.3 关键方法

```cpp
// 新对象的初始 mark word：hash=0, age=0, 无锁 = 0x0000000000000001
static markOopDesc* prototype() {
    return (markOopDesc*)(intptr_t)(no_hash_in_place | no_lock_in_place);
}

// 设置 hash（不修改其他位）
markOopDesc* copy_set_hash(intptr_t hash) const {
    intptr_t tmp = value() & (~hash_mask_in_place);
    tmp |= ((hash & hash_mask) << hash_shift);
    return (markOopDesc*)tmp;
}

// GC 标记
markOopDesc* set_marked() {
    return (markOopDesc*)((value() & ~lock_mask_in_place) | marked_value);
}
```

#### 3.1.4 与 HotSpot 的差异

| 方面 | HotSpot | 我们的版本 | 原因 |
|------|---------|-----------|------|
| 继承 | `markOopDesc : public oopDesc` | 独立类 | 避免循环依赖 |
| 偏向锁操作 | 完整实现 | 仅保留模式判断 | 后续需要时添加 |
| CAS 操作 | 有 `cmpxchg` 等 | 暂无 | 并发阶段实现 |

---

### 3.2 oop.hpp — Java 对象基类

#### 3.2.1 oopDesc 内存布局

```
                oopDesc (16 bytes on LP64)
 Offset   Size   Field
┌────────┬──────┬─────────────────────────────────┐
│ 0      │ 8B   │ volatile markOop _mark          │ ← Mark Word
├────────┼──────┼─────────────────────────────────┤
│ 8      │ 8B   │ union { Klass* _klass;          │ ← Klass 指针
│        │      │         juint _compressed_klass; │   （压缩时仅 4B）
│        │      │       } _metadata               │
├────────┼──────┼─────────────────────────────────┤
│ 16     │ ...  │ [instance fields start here]    │
└────────┴──────┴─────────────────────────────────┘
```

关键设计决策：
- **没有 C++ 虚表指针** — oopDesc 没有虚函数，所以 sizeof(oopDesc) = 16，不是 24
- **`_mark` 是 volatile** — 锁操作需要 CAS
- **`_metadata` 是 union** — 非压缩时 Klass*（8B），压缩时 narrowKlass（4B）

#### 3.2.2 关键数值

```
sizeof(oopDesc)         = 16 bytes
header_size()           = 16 / 8 = 2 HeapWords
mark_offset_in_bytes()  = 0
klass_offset_in_bytes() = 8
```

#### 3.2.3 字段访问

字段通过字节偏移直接访问，不通过虚函数：

```cpp
void* field_addr(int offset) const {
    return (void*)((address)this + offset);
}

jint int_field(int offset) const { return *(jint*)field_addr(offset); }
```

这是 JVM 访问对象字段的核心方式——已知字段偏移后，直接用指针偏移读写。

---

### 3.3 instanceOop.hpp — 普通 Java 对象实例

最简单的文件。`instanceOopDesc` 继承 `oopDesc`，不添加任何字段：

```cpp
class instanceOopDesc : public oopDesc {
public:
    static int header_size() { return sizeof(instanceOopDesc) / HeapWordSize; } // = 2
    static int base_offset_in_bytes() { return sizeof(instanceOopDesc); }       // = 16
};
```

实例字段从偏移 16 开始。

---

### 3.4 metadata.hpp — 类元数据基类

#### 3.4.1 继承层次

```
CHeapObj<mtClass>  (暂代 MetaspaceObj)
  └── Metadata
        ├── Klass
        │   └── InstanceKlass
        ├── Method
        └── ConstantPool
```

#### 3.4.2 核心设计

Metadata 提供虚函数用于类型判断：

```cpp
virtual bool is_klass()        const { return false; }
virtual bool is_method()       const { return false; }
virtual bool is_constantPool() const { return false; }
```

这就是 OOP-Klass 二元结构的关键：
- oopDesc **没有**虚函数 → 省空间
- Metadata（Klass/Method 等）**有**虚函数 → 提供多态行为

后续引入 Metaspace 时，`CHeapObj<mtClass>` 将替换为 `MetaspaceObj`。

---

### 3.5 accessFlags.hpp — 访问标志

#### 3.5.1 标志定义

分为三层：

**Java 标准标志（低 15 位）：**

| 常量 | 值 | 含义 |
|------|------|------|
| JVM_ACC_PUBLIC | 0x0001 | public |
| JVM_ACC_PRIVATE | 0x0002 | private |
| JVM_ACC_PROTECTED | 0x0004 | protected |
| JVM_ACC_STATIC | 0x0008 | static |
| JVM_ACC_FINAL | 0x0010 | final |
| JVM_ACC_SYNCHRONIZED | 0x0020 | synchronized |
| JVM_ACC_SUPER | 0x0020 | ACC_SUPER（类标志，与 synchronized 同值） |
| JVM_ACC_VOLATILE | 0x0040 | volatile |
| JVM_ACC_BRIDGE | 0x0040 | bridge method |
| JVM_ACC_TRANSIENT | 0x0080 | transient |
| JVM_ACC_VARARGS | 0x0080 | varargs method |
| JVM_ACC_NATIVE | 0x0100 | native |
| JVM_ACC_INTERFACE | 0x0200 | interface |
| JVM_ACC_ABSTRACT | 0x0400 | abstract |
| JVM_ACC_STRICT | 0x0800 | strictfp |
| JVM_ACC_SYNTHETIC | 0x1000 | synthetic |
| JVM_ACC_ANNOTATION | 0x2000 | annotation |
| JVM_ACC_ENUM | 0x4000 | enum |

**HotSpot 扩展标志（高位）：**

| 常量 | 值 | 上下文 | 含义 |
|------|------|--------|------|
| JVM_ACC_HAS_FINALIZER | 0x40000000 | Klass | 有 finalize() 方法 |
| JVM_ACC_IS_CLONEABLE_FAST | 0x80000000 | Klass | 可快速 clone |
| JVM_ACC_QUEUED | 0x00010000 | Method | 已加入编译队列 |
| JVM_ACC_NOT_C2_COMPILABLE | 0x02000000 | Method | 不能用 C2 编译 |
| JVM_ACC_FIELD_HAS_GENERIC_SIGNATURE | 0x00000800 | Field | 有泛型签名 |

**用途区分：**
同一个位在 Class/Method/Field 上下文中含义不同（如 0x0020 对类是 SUPER，对方法是 SYNCHRONIZED）。AccessFlags 类统一处理这种多态。

---

### 3.6 klass.hpp — 类元数据基类

#### 3.6.1 核心字段

| 字段 | 类型 | 作用 |
|------|------|------|
| `_layout_helper` | jint | 对象大小（正数=实例字节数，负数=数组描述符，0=neutral） |
| `_id` | KlassID | 子类标识（6 种） |
| `_super_check_offset` | juint | 快速子类型检查的偏移 |
| `_name` | const char* | 类名（如 "java/lang/String"） |
| `_primary_supers[8]` | Klass*[] | 父类缓存（8 层以内直接查表） |
| `_super` | Klass* | 直接父类 |
| `_access_flags` | AccessFlags | 访问修饰符 |
| `_prototype_header` | markOop | 偏向锁原型头 |
| `_vtable_len` | int | Java 虚方法表长度 |

#### 3.6.2 Layout Helper 编码

```
Instance Klass:
  layout_helper = instance_size_in_bytes（正数，已对齐到 HeapWord）
  例：HelloWorld → layout_helper = 24 (16B header + 4B int + 4B padding)

Array Klass:
  layout_helper < 0（负数编码）
  格式：[array_tag(8) | header_size(8) | element_type(8) | log2_element_size(8)]
```

类型判断：
```cpp
bool is_instance_klass() const { return _layout_helper > 0; }
bool is_array_klass()    const { return _layout_helper < 0; }
```

#### 3.6.3 Primary Supers 快速子类型检查

8 层以内的继承关系直接查表，O(1) 时间复杂度。超过 8 层则需要遍历（极少发生）。

---

### 3.7 constMethod.hpp — 方法的不可变部分

#### 3.7.1 核心概念

方法数据分为两部分：
- **ConstMethod**（不可变）：字节码、行号表、异常表、名称/签名索引等
- **Method**（可变）：访问标志、vtable 索引、入口点、编译状态等

分离原因：ConstMethod 可以被 CDS（Class Data Sharing）跨进程共享。

#### 3.7.2 HotSpot 的内嵌布局 vs 我们的简化

HotSpot 把字节码和各种表直接内嵌在 ConstMethod 对象之后：

```
HotSpot 内存布局：
┌───────────────────────────┐
│ ConstMethod 固定字段       │ ← this
├───────────────────────────┤
│ [EMBEDDED bytecodes]      │ ← code_base()
│    (code_size bytes)      │
├───────────────────────────┤
│ [line number table]       │ (从前往后)
├───────────────────────────┤
│ [exception table]         │ (从后往前索引)
│ [checked exceptions]      │
│ [method parameters]       │
└───────────────────────────┘
```

我们的简化版用独立的堆分配数组存储字节码，后续可改为内嵌。

#### 3.7.3 关键字段

| 字段 | 类型 | 作用 |
|------|------|------|
| `_constants` | ConstantPool* | 回指所属常量池 |
| `_code_size` | u2 | 字节码长度 |
| `_max_stack` | u2 | 操作数栈最大深度 |
| `_max_locals` | u2 | 局部变量表大小 |
| `_name_index` | u2 | 方法名（常量池索引） |
| `_signature_index` | u2 | 方法签名（常量池索引） |
| `_method_idnum` | u2 | 方法在类中的唯一编号 |
| `_bytecodes` | u1* | 字节码数组 |

---

### 3.8 method.hpp — 方法元数据

#### 3.8.1 内存布局

```
Method 对象布局：
┌──────────────────────────────┐
│ Metadata vptr (8B)           │ C++ 虚表指针
├──────────────────────────────┤
│ _constMethod (ConstMethod*)  │ 指向不可变部分
│ _access_flags (AccessFlags)  │ 访问标志
│ _vtable_index (int)          │ 虚表索引
│ _intrinsic_id (u2)           │ 内建方法 ID
│ _flags (u2)                  │ 内部标志
│ _i2i_entry (address)         │ 解释器入口
│ _from_compiled_entry (addr)  │ 编译代码入口
│ _from_interpreted_entry (addr)│ 解释器→编译代码
│ _native_function (address)   │ native 方法函数指针
│ _signature_handler (address) │ native 签名处理器
└──────────────────────────────┘
```

#### 3.8.2 vtable_index 特殊值

| 值 | 含义 |
|------|------|
| >= 0 | vtable 中的索引位置 |
| -2 (nonvirtual) | 非虚方法（private/static/final/`<init>`） |
| -3 (pending_itable) | itable 索引待计算 |
| -4 (invalid) | 初始值，尚未设置 |

---

### 3.9 instanceKlass.hpp/.cpp — Java 类的完整元数据

#### 3.9.1 ClassState 状态机

```
allocated → loaded → linked → being_initialized → fully_initialized
                                                 → initialization_error
```

| 状态 | 含义 |
|------|------|
| allocated | 已分配内存，尚未填充 |
| loaded | 已加载并插入类层次 |
| linked | 已链接（验证+准备+解析） |
| being_initialized | 正在执行 `<clinit>` |
| fully_initialized | 初始化完成，可以使用 |
| initialization_error | 初始化出错 |

#### 3.9.2 FieldInfoEntry

```cpp
struct FieldInfoEntry {
    u2 access_flags;          // 访问标志
    u2 name_index;            // 字段名（常量池索引）
    u2 descriptor_index;      // 描述符（常量池索引）
    u2 offset;                // 实例偏移（static 字段为 0xFFFF）
    u2 constant_value_index;  // ConstantValue 属性
};
```

#### 3.9.3 实例大小计算

`create_from_parser()` 中的字段布局算法：

```
instance_size = instanceOopDesc::base_offset_in_bytes()  // = 16

对每个非 static 字段：
  根据描述符确定 field_size：
    J/D (long/double)      → 8B，8 对齐
    L/[ (引用/数组)        → oopSize=8B，8 对齐
    B/Z (byte/boolean)     → 1B
    S/C (short/char)       → 2B，2 对齐
    其他 (I/F)             → 4B
  字段偏移 = 当前 instance_size（对齐后）
  instance_size += field_size

最终对齐到 HeapWord (8 字节)
```

HelloWorld 的例子：
```
header:           16 bytes (oopDesc)
count (int):       4 bytes → offset=16
                  padding: 4 bytes (对齐到 HeapWord)
───────────────
total:            24 bytes = 3 HeapWords

MESSAGE (static): 不占实例空间
```

#### 3.9.4 所有权模型

```
InstanceKlass 拥有：
  ├── ConstantPool*   (_constants)    → 析构时 delete
  ├── Method**        (_methods)      → 析构时逐个 delete
  │   └── Method*
  │       └── ConstMethod*            → Method 析构时 delete
  │           └── u1* _bytecodes      → ConstMethod 析构时 FREE
  ├── FieldInfoEntry* (_field_infos)  → 析构时 FREE
  └── char*           (_class_name)   → 析构时 FREE
      char*           (_super_class_name) → 析构时 FREE
```

---

## 四、Phase 2→3 衔接：create_instance_klass()

这是整个 Phase 3 的核心衔接点，将 ClassFileParser 的临时数据转化为正式的 OOP-Klass 对象。

### 4.1 转换流程

```
ClassFileParser 临时数据              OOP-Klass 正式对象
┌───────────────┐                    ┌──────────────────┐
│ FieldInfo[]   │ ──转换──→          │ FieldInfoEntry[] │
│  .access_flags│                    │  .offset 已计算   │
│  .name_index  │                    └──────────────────┘
│  .code        │
└───────────────┘

┌───────────────┐     拷贝bytecodes    ┌────────────────┐
│ MethodInfo[]  │ ──→ ConstMethod[] ──→│ Method*[]       │
│  .code        │     (字节码拷贝)      │  ._constMethod  │
│  .max_stack   │     释放原始code      │  ._access_flags │
└───────────────┘                      └────────────────┘

┌───────────────┐     转移所有权        ┌────────────────┐
│ ConstantPool* │ ──→ (_cp = nullptr) →│ InstanceKlass   │
│ (_cp)         │                      │  ._constants    │
└───────────────┘                      └────────────────┘
```

### 4.2 实现（classFileParser.cpp）

```cpp
InstanceKlass* ClassFileParser::create_instance_klass() {
    // 1. FieldInfo[] → FieldInfoEntry[]（堆分配，偏移由 create_from_parser 计算）
    // 2. MethodInfo[] → Method*[]（每个 Method 包含新建的 ConstMethod）
    //    - 字节码从 MethodInfo.code 拷贝到 ConstMethod._bytecodes
    //    - 释放 MethodInfo.code 原始内存
    // 3. 调用 InstanceKlass::create_from_parser()
    // 4. 转移 ConstantPool 所有权（_cp = nullptr）
    return ik;
}
```

---

## 五、测试结果

### 5.1 测试清单

| 测试 | 内容 | 结果 |
|------|------|------|
| test_bytes | Phase 1 字节序测试 | PASS |
| test_classfile_stream | Phase 2 流读取测试 | PASS |
| test_mark_word | Mark Word 位布局、锁状态、age、hash | PASS |
| test_oop_header | oopDesc 大小=16B、header_size=2、偏移 | PASS |
| test_access_flags | 标准/扩展标志查询 | PASS |
| test_metadata_method | Metadata 虚派发、Method/ConstMethod | PASS |
| test_klass | InstanceKlass 创建、状态转换 | PASS |
| test_full_pipeline | .class → parse → InstanceKlass → find_method | PASS |

### 5.2 关键验证数据

```
=== Mark Word ===
  prototype = 0x0000000000000001
  lock_bits=2, biased_lock_bits=1, age_bits=4, hash_bits=31
  hash_mask=0x000000007fffffff

=== oopDesc ===
  sizeof(oopDesc) = 16 bytes
  header_size() = 2 HeapWords
  mark_offset = 0, klass_offset = 8

=== HelloWorld InstanceKlass ===
  Instance size: 24 bytes (3 HeapWords)
  Fields:
    [0] I count (offset=16)
    [1] Ljava/lang/String; MESSAGE (static)
  Methods:
    [0] <init>()V (code_size=10)
    [1] add(II)I (code_size=4)
    [2] main([Ljava/lang/String;)V (code_size=23)
```

### 5.3 内存安全

通过 valgrind 验证零内存泄漏：
```
HEAP SUMMARY:
  in use at exit: 0 bytes in 0 blocks
  total heap usage: 62 allocs, 62 frees, 84,055 bytes allocated
  All heap blocks were freed -- no leaks are possible
ERROR SUMMARY: 0 errors from 0 contexts
```

---

## 六、编译修复记录

| 问题 | 原因 | 修复 |
|------|------|------|
| `right_n_bits()` 不是 constexpr | enum 初始化需要编译期常量 | 将 `nth_bit`/`right_n_bits` 改为 `constexpr inline` |
| `BitsPerLong` 未定义 | globalDefinitions.hpp 缺少该常量 | 添加 `BitsPerLong = BytesPerLong * BitsPerByte` |
| MethodInfo.code 内存泄漏（37B） | create_instance_klass 中只设 nullptr 未释放 | 拷贝后 FREE 原始内存再置 nullptr |

---

## 七、当前项目结构

```
my_jvm_11/
├── CMakeLists.txt
├── docs/
│   ├── Phase-01-Foundation.md
│   ├── Phase-02-ClassFileParser.md
│   └── Phase-03-OOP-Klass.md          ← 本文档
├── src/share/
│   ├── classfile/
│   │   ├── classFileParser.cpp/.hpp    (修改)
│   │   └── classFileStream.hpp
│   ├── memory/
│   │   └── allocation.hpp/.cpp
│   ├── oops/
│   │   ├── constMethod.hpp             ← Phase 3 新增
│   │   ├── constantPool.hpp/.cpp       (修改)
│   │   ├── instanceKlass.hpp/.cpp      ← Phase 3 新增
│   │   ├── instanceOop.hpp             ← Phase 3 新增
│   │   ├── klass.hpp                   ← Phase 3 新增
│   │   ├── markOop.hpp                 ← Phase 3 新增
│   │   ├── metadata.hpp                ← Phase 3 新增
│   │   ├── method.hpp                  ← Phase 3 新增
│   │   ├── oop.hpp                     ← Phase 3 新增
│   │   └── oopsHierarchy.hpp           (修改)
│   └── utilities/
│       ├── accessFlags.hpp             ← Phase 3 新增
│       ├── bytes.hpp
│       ├── constantTag.hpp
│       ├── debug.hpp
│       ├── globalDefinitions.hpp       (修改)
│       └── macros.hpp
├── test/
│   ├── HelloWorld.class
│   ├── HelloWorld.java
│   └── main.cpp                        (重写)
└── build/
    └── mini_jvm                        ← 可执行文件
```

---

## 八、下一步：Phase 4 — 线程与栈帧

Phase 3 完成后，我们有了完整的类元数据表示。但要执行字节码，还需要：

1. **JavaThread** — 线程数据结构
2. **Frame / Stack** — 栈帧，存储局部变量表和操作数栈
3. **字节码解释器** — 逐条执行字节码

```
Phase 4 目标：
  .class → parse → InstanceKlass → 创建 Thread → 创建栈帧 → 执行字节码
```
