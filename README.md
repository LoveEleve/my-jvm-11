# my-jvm-11

> 基于 OpenJDK 11 源码，用 C++ 从零实现一个 JVM。
> 目标：**核心机制高度还原**——数据结构与算法设计与 OpenJDK 11 高度一致。

---

## 项目目标

不追求"能跑就行"，追求**每个核心机制都按照 OpenJDK 11 的真实设计实现**：

- 数据结构与 OpenJDK 11 对齐（字段含义、内存布局、生命周期）
- 算法与 OpenJDK 11 对齐（核心路径、状态机、关键设计决策）
- 每个模块都有对应的 OpenJDK 11 源码作为参考依据

## 参考来源

- OpenJDK 11 源码：`/data/workspace/openjdk-cut-new/src/hotspot/`
- 源码深度分析文档：`/data/workspace/openjdk-cut-new/new-jvm-md/`

---

## 核心模块规划

### 模块 1：对象体系（oops）⬜
还原目标：`oop` / `Klass` / `InstanceKlass` / `Method` / `ConstantPool`

| 机制 | OpenJDK 11 参考 | 还原要点 |
|------|----------------|---------|
| 对象头 MarkWord | `markOop.hpp` | 锁状态位域、hashCode、age 编码完全一致 |
| Klass 体系 | `klass.hpp` / `instanceKlass.hpp` | vtable/itable 布局、OopMapBlock |
| Method 元数据 | `method.hpp` | `_vtable_index` 双编码、InvocationCounter |
| ConstantPool | `constantPool.hpp` | tag 数组 + 数据数组双轨设计 |

### 模块 2：类加载（classfile）⬜
还原目标：`.class` 文件解析 → `InstanceKlass` 构建

| 机制 | OpenJDK 11 参考 | 还原要点 |
|------|----------------|---------|
| ClassFile 解析 | `classFileParser.cpp` | 魔数/版本/常量池/字段/方法完整解析 |
| InstanceKlass 分配 | `instanceKlass.cpp` | vtable/itable/OopMapBlock 紧跟 Klass 内存布局 |
| 类加载器体系 | `classLoader.cpp` | Bootstrap/App 双层结构 |
| 类初始化状态机 | `instanceKlass.cpp` | 6 态状态机（linked→being_initialized→fully_initialized） |

### 模块 3：字节码解释器（interpreter）⬜
还原目标：基于栈的字节码解释执行

| 机制 | OpenJDK 11 参考 | 还原要点 |
|------|----------------|---------|
| 栈帧结构 | `frame.hpp` | locals + operand_stack + constant_pool_cache |
| 方法调用 | `bytecodeInterpreter.cpp` | invokevirtual vtable 查找、invokeinterface itable 查找 |
| 异常处理 | `bytecodeInterpreter.cpp` | ExceptionTable 线性扫描 |
| 核心指令集 | JVM Spec | 覆盖 200+ 核心字节码 |

### 模块 4：内存管理（memory）⬜
还原目标：堆分配 + 基础 GC

| 机制 | OpenJDK 11 参考 | 还原要点 |
|------|----------------|---------|
| Region 设计 | `heapRegion.hpp` | Region 固定大小、Eden/Survivor/Old 分类 |
| TLAB 分配 | `threadLocalAllocBuffer.hpp` | bump pointer 快速分配路径 |
| 对象分配 | `memAllocator.cpp` | 慢速路径 → 触发 GC |
| 标记-清除 GC | `g1CollectedHeap.cpp` | 根扫描 → 标记 → 清除（简化版） |

### 模块 5：运行时（runtime）⬜
还原目标：线程模型 + 同步机制

| 机制 | OpenJDK 11 参考 | 还原要点 |
|------|----------------|---------|
| JavaThread | `thread.hpp` | 线程状态机（_thread_in_Java/_thread_in_vm 等） |
| ObjectMonitor | `objectMonitor.cpp` | enter/exit/wait/notify 完整实现 |
| 偏向锁/轻量级锁 | `synchronizer.cpp` | MarkWord CAS 升级路径 |
| SafePoint | `safepoint.cpp` | 轮询机制、stop-the-world |

---

## 实现原则

1. **数据结构优先**：每个模块先完整实现数据结构，再实现算法
2. **对齐 OpenJDK**：字段命名、内存布局、状态机与 OpenJDK 11 保持一致
3. **有据可查**：每个设计决策都能在 OpenJDK 11 源码中找到对应位置
4. **可验证**：每个模块都有对应的测试用例

---

## 构建

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

---

## 目录结构

```
my_jvm/
├── src/
│   ├── oops/           # 对象体系：oop / Klass / InstanceKlass / Method
│   ├── classfile/      # 类加载：.class 解析 → InstanceKlass 构建
│   ├── interpreter/    # 字节码解释器：栈帧 + 指令集
│   ├── memory/         # 内存管理：Region / TLAB / GC
│   ├── runtime/        # 运行时：线程 / ObjectMonitor / SafePoint
│   └── main.cpp        # 入口
├── test/               # 测试用 .class 文件
├── docs/               # 每个模块的设计说明（对应 OpenJDK 11 源码位置）
├── CMakeLists.txt
└── README.md
```

---

## 进度

| 模块 | 状态 | 备注 |
|------|------|------|
| oops 对象体系 | ⬜ 未开始 | |
| classfile 类加载 | ⬜ 未开始 | |
| interpreter 解释器 | ⬜ 未开始 | |
| memory 内存管理 | ⬜ 未开始 | |
| runtime 运行时 | ⬜ 未开始 | |
