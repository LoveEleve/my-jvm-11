# my-jvm-11

> 基于 OpenJDK 11 源码，用 C++ 从零实现一个 JVM。
> 目标：**核心机制高度还原**——数据结构与算法设计与 OpenJDK 11 高度一致。

---

## 项目目标

不追求"能跑就行"，追求**每个核心机制都按照 OpenJDK 11 的真实设计实现**：

- 数据结构与 OpenJDK 11 对齐（字段含义、内存布局、生命周期）
- 算法与 OpenJDK 11 对齐（核心路径、状态机、关键设计决策）
- 每个模块都有对应的 OpenJDK 11 源码分析文档作为参考依据

## 参考来源

- OpenJDK 11 源码：`/data/workspace/openjdk-cut-new/src/hotspot/`
- 源码深度分析文档：`/data/workspace/openjdk-cut-new/new-jvm-md/`

---

## 核心模块大纲

### 模块 0：基础设施（utilities）⬜

JVM 内部使用的基础数据结构，是其他所有模块的地基。

| 机制 | OpenJDK 11 参考 | 还原要点 |
|------|----------------|---------|
| Symbol / SymbolTable | `symbol.hpp` / `symbolTable.hpp` | 字符串驻留、ConcurrentHashTable 实现 |
| Arena 内存分配器 | `allocation.hpp` | 块式内存池，避免频繁 malloc |
| GrowableArray | `growableArray.hpp` | 可扩容数组，类似 std::vector |
| ResourceMark / HandleMark | `handles.hpp` | 栈式内存管理，自动释放 |
| tty 输出 | `ostream.hpp` | JVM 内部日志输出机制 |

---

### 模块 1：JVM 启动（init）⬜

还原目标：`JNI_CreateJavaVM` → `Threads::create_vm` 完整启动流程

| 机制 | OpenJDK 11 参考 | 还原要点 |
|------|----------------|---------|
| 参数解析 | `arguments.cpp` | `-Xms/-Xmx/-XX:+UseG1GC` 等参数解析 |
| OS 初始化 | `os_linux.cpp` | 页大小、CPU 数量、信号注册 |
| Universe 初始化 | `universe.cpp` | 全局单例、堆创建入口 |
| 核心类初始化 | `init.cpp` | `java.lang.Object/String/Class` 等核心类加载顺序 |
| VMThread 启动 | `vmThread.cpp` | VM 操作线程创建与事件循环 |
| 主线程附加 | `thread.cpp` | JavaThread 创建、JNIHandleBlock 初始化 |

---

### 模块 2：对象体系（oops）⬜

还原目标：`oop` / `Klass` / `InstanceKlass` / `Method` / `ConstantPool`

| 机制 | OpenJDK 11 参考 | 还原要点 |
|------|----------------|---------|
| 对象头 MarkWord | `markOop.hpp` | 5 态位域编码：无锁/偏向/轻量/重量/GC标记 |
| Klass 体系 | `klass.hpp` | Klass → InstanceKlass → ArrayKlass 继承体系 |
| InstanceKlass 布局 | `instanceKlass.hpp` | vtable/itable/OopMapBlock 紧跟 Klass 内存布局 |
| Method 元数据 | `method.hpp` | `_vtable_index` 双编码（vtable索引/itable偏移）、InvocationCounter |
| ConstantPool | `constantPool.hpp` | tag 数组 + 数据数组双轨设计、运行时解析 |
| CompressedOops | `compressedOops.hpp` | 32位压缩指针、零基/非零基编解码算法 |

---

### 模块 3：类加载（classfile）⬜

还原目标：`.class` 文件解析 → `InstanceKlass` 构建 → 链接 → 初始化

| 机制 | OpenJDK 11 参考 | 还原要点 |
|------|----------------|---------|
| ClassFile 解析 | `classFileParser.cpp` | 魔数/版本/常量池/字段/方法/属性完整解析 |
| InstanceKlass 分配 | `instanceKlass.cpp` | vtable/itable/OopMapBlock 内存紧跟 Klass 布局 |
| 类加载器体系 | `classLoader.cpp` | Bootstrap/Extension/App 三层委派模型 |
| SystemDictionary | `systemDictionary.cpp` | 类注册表、ConcurrentHashTable 存储 |
| ClassLoaderData | `classLoaderData.hpp` | 每个 ClassLoader 独立的元数据空间 |
| 类初始化状态机 | `instanceKlass.cpp` | 6 态状态机（allocated→loaded→linked→being_initialized→fully_initialized→error） |
| vtable 构建 | `klassVtable.cpp` | vtable 大小计算、方法槽分配、override 检测 |
| itable 构建 | `klassVtable.cpp` | 接口方法表构建、itableOffsetEntry + itableMethodEntry |

---

### 模块 4：内存管理（memory）⬜

还原目标：G1 堆结构 + TLAB 分配 + 基础 GC

| 机制 | OpenJDK 11 参考 | 还原要点 |
|------|----------------|---------|
| HeapRegion | `heapRegion.hpp` | 固定大小 Region、Eden/Survivor/Old/Humongous 分类、`_type` 状态机 |
| HeapRegionManager | `heapRegionManager.hpp` | Region 数组管理、空闲列表 |
| TLAB | `threadLocalAllocBuffer.hpp` | bump pointer 快速分配、`_top/_end/_start` 三指针 |
| 对象分配全链路 | `memAllocator.cpp` | TLAB 命中 → Eden 慢速路径 → 触发 GC |
| 大对象分配 | `g1CollectedHeap.cpp` | Humongous 对象（>Region/2）直接分配 |
| G1BarrierSet | `g1BarrierSet.hpp` | 写前/写后屏障、CardTable 标记 |
| G1CardTable | `g1CardTable.hpp` | 512字节卡粒度、dirty card 追踪 |
| G1RemSet | `g1RemSet.hpp` | 跨 Region 引用记录、RefinementThread 异步处理 |
| Metaspace | `metaspace.hpp` | 类元数据内存、Chunk/Block 分配器 |
| OopStorage | `oopStorage.hpp` | JNI 全局引用、弱引用存储 |

---

### 模块 5：执行引擎（interpreter）⬜

还原目标：基于栈的字节码解释执行

| 机制 | OpenJDK 11 参考 | 还原要点 |
|------|----------------|---------|
| 栈帧结构 | `frame.hpp` | locals + operand_stack + constant_pool_cache_ptr + return_addr |
| 解释器栈帧布局 | `interpreterRuntime.cpp` | 解释器帧 vs 编译帧的内存布局差异 |
| 方法调用分派 | `bytecodeInterpreter.cpp` | invokevirtual/invokeinterface/invokespecial/invokestatic |
| vtable 查找 | `klassVtable.cpp` | `vtable[index]` 直接索引 |
| itable 查找 | `klassVtable.cpp` | 接口 ID 线性扫描 → 方法偏移 |
| ConstantPoolCache | `cpCache.hpp` | 方法调用缓存、首次解析后缓存结果 |
| 异常处理 | `bytecodeInterpreter.cpp` | ExceptionTable 线性扫描、栈展开 |
| 核心指令集 | JVM Spec | 覆盖 load/store/arithmetic/control/invoke/return 全类别 |
| InvocationCounter | `invocationCounter.hpp` | 方法调用计数、热点检测阈值 |

---

### 模块 6：线程系统（thread）⬜

还原目标：线程模型 + 线程状态机 + SafePoint

| 机制 | OpenJDK 11 参考 | 还原要点 |
|------|----------------|---------|
| JavaThread | `thread.hpp` | 线程状态机（5态：_thread_new/_in_Java/_in_vm/_in_native/_blocked） |
| OSThread | `osThread.hpp` | 平台线程句柄、pthread_t 封装 |
| 线程栈管理 | `thread.cpp` | 栈基址/大小、guard page 设置 |
| JNIHandleBlock | `jniHandles.hpp` | 局部引用块、链表管理 |
| VMThread | `vmThread.cpp` | VM 操作队列、事件循环、VMOperation 执行 |
| SafePoint | `safepoint.cpp` | 轮询页机制、stop-the-world 协调 |
| Handshake | `handshake.hpp` | 单线程握手（无需全局 STW） |
| WatcherThread | `watcherThread.hpp` | 定时任务调度 |
| ServiceThread | `serviceThread.hpp` | 后台服务（GC 通知、JVMTI 事件） |

---

### 模块 7：同步机制（synchronization）⬜

还原目标：偏向锁 → 轻量级锁 → 重量级锁完整升级路径

| 机制 | OpenJDK 11 参考 | 还原要点 |
|------|----------------|---------|
| MarkWord 锁状态 | `markOop.hpp` | 5 态位域：无锁(001)/偏向(101)/轻量(00)/重量(10)/GC(11) |
| 偏向锁 | `biasedLocking.cpp` | 线程ID写入MarkWord、批量撤销机制 |
| 轻量级锁 | `synchronizer.cpp` | Lock Record CAS、栈上锁记录 |
| 重量级锁膨胀 | `synchronizer.cpp` | inflate → ObjectMonitor 分配 |
| ObjectMonitor | `objectMonitor.cpp` | `_EntryList`/`_WaitSet` 双队列、enter/exit/wait/notify |
| Parker | `park.hpp` | pthread_mutex + pthread_cond 封装、park/unpark |

---

### 模块 8：GC 实现（gc）⬜

还原目标：G1 Young GC + Mixed GC 核心流程

| 机制 | OpenJDK 11 参考 | 还原要点 |
|------|----------------|---------|
| GC 触发条件 | `g1CollectedHeap.cpp` | Eden 满触发 Young GC、IHOP 阈值触发 Mixed GC |
| SafePoint 进入 | `safepoint.cpp` | STW 开始、所有线程到达安全点 |
| 根扫描 | `g1RootProcessor.cpp` | 线程栈/JNI引用/静态变量/CardTable 根集合 |
| 对象复制（疏散） | `g1ParScanThreadState.cpp` | 存活对象复制到 Survivor/Old Region |
| 引用处理 | `referenceProcessor.cpp` | 软/弱/虚/终结引用四类处理 |
| 并发标记 | `g1ConcurrentMark.cpp` | SATB 写屏障、三色标记、并发标记线程 |
| Mixed GC 选集 | `g1CollectionSet.cpp` | 基于 G1Policy 选择 Old Region |
| G1Policy | `g1Policy.cpp` | 停顿时间预测、自适应调整 |
| ReferenceProcessor | `referenceProcessor.cpp` | 软/弱/虚/终结引用处理链 |

---

### 模块 9：JIT 编译（compiler）⬜

还原目标：热点检测 → 编译触发 → 代码安装（简化版）

| 机制 | OpenJDK 11 参考 | 还原要点 |
|------|----------------|---------|
| 热点检测 | `invocationCounter.hpp` | InvocationCounter 计数、CompileThreshold 阈值 |
| CompileBroker | `compileBroker.cpp` | 编译任务队列、C1/C2 编译线程调度 |
| CodeCache | `codeCache.hpp` | 编译代码存储、NonNMethod/Profiled/NonProfiled 三段 |
| OSR 替换 | `compileBroker.cpp` | 循环热点检测、栈上替换 |
| 去优化 | `deoptimization.cpp` | 编译代码失效、回退到解释执行 |
| 内联缓存 | `inlineCache.hpp` | 单态/多态/超多态调用点缓存 |

---

### 模块 10：运行时服务（runtime）⬜

还原目标：异常处理 + JNI + 信号处理

| 机制 | OpenJDK 11 参考 | 还原要点 |
|------|----------------|---------|
| 异常处理 | `exceptions.cpp` | 异常对象创建、栈展开、ExceptionTable 查找 |
| JNI 调用约定 | `jni.cpp` | JNI 函数表、局部引用管理、类型转换 |
| 信号处理链 | `libjsig.so` | LD_PRELOAD 拦截、JVM 信号优先 |
| StubRoutines | `stubRoutines.hpp` | 汇编桩代码：数组复制/类型检查/SafePoint 轮询 |
| 字符串驻留 | `stringTable.hpp` | StringTable ConcurrentHashTable、intern 机制 |
| 反射 | `javaClasses.cpp` | `java.lang.reflect.Method` 与 `Method*` 映射 |

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
│   ├── utilities/      # 基础设施：Symbol / Arena / GrowableArray
│   ├── init/           # JVM 启动：create_vm 流程
│   ├── oops/           # 对象体系：oop / Klass / InstanceKlass / Method
│   ├── classfile/      # 类加载：.class 解析 → InstanceKlass 构建
│   ├── memory/         # 内存管理：Region / TLAB / Metaspace
│   ├── interpreter/    # 字节码解释器：栈帧 + 指令集
│   ├── thread/         # 线程系统：JavaThread / VMThread / SafePoint
│   ├── synchronization/# 同步机制：偏向锁 / 轻量锁 / ObjectMonitor
│   ├── gc/             # GC 实现：G1 Young GC / Mixed GC
│   ├── compiler/       # JIT 编译：热点检测 / CompileBroker / CodeCache
│   ├── runtime/        # 运行时服务：异常 / JNI / 信号
│   └── main.cpp        # 入口
├── test/               # 测试用 .class 文件
├── docs/               # 每个模块的设计说明（对应 OpenJDK 11 源码位置）
├── CMakeLists.txt
└── README.md
```

---

## 进度

| 模块 | 状态 | 参考文档 |
|------|------|---------|
| 0. utilities 基础设施 | ⬜ 未开始 | `new-jvm-md/ConcurrentHashTable/` |
| 1. init JVM 启动 | ⬜ 未开始 | `new-jvm-md/JVM-Startup/` |
| 2. oops 对象体系 | ⬜ 未开始 | `new-jvm-md/JVM-Core-Objects/04-MarkWord-Encoding.md` `07-InstanceKlass-Layout.md` |
| 3. classfile 类加载 | ⬜ 未开始 | `new-jvm-md/ClassLoading/` `new-jvm-md/JVM-Core-Objects/06-ClassLoading-Timeline.md` |
| 4. memory 内存管理 | ⬜ 未开始 | `new-jvm-md/G1GC/` `new-jvm-md/JVM-Startup/Universe/` |
| 5. interpreter 解释器 | ⬜ 未开始 | `new-jvm-md/Interpreter/` `new-jvm-md/JVM-Core-Objects/02-MethodInvocation-Full-Chain.md` |
| 6. thread 线程系统 | ⬜ 未开始 | `new-jvm-md/Thread/` `new-jvm-md/JVM-Startup/Phase3/` |
| 7. synchronization 同步 | ⬜ 未开始 | `new-jvm-md/Synchronization/` `new-jvm-md/JVM-Core-Objects/05-ObjectMonitor-Deep-Dive.md` |
| 8. gc GC 实现 | ⬜ 未开始 | `new-jvm-md/G1GC/` |
| 9. compiler JIT | ⬜ 未开始 | `new-jvm-md/Compiler/` `new-jvm-md/InvocationCounter/` |
| 10. runtime 运行时服务 | ⬜ 未开始 | `new-jvm-md/ExceptionHandling/` `new-jvm-md/SOLibrary/` |
