# my_jvm

> 从零开始用 C++ 实现一个 JVM，参考 OpenJDK 11 源码。

## 项目目标

基于对 OpenJDK 11 源码的深度分析，从零实现一个可运行 Java `.class` 文件的 JVM。
不追求完整性，追求**对核心机制的深度理解与正确实现**。

## 参考来源

- OpenJDK 11 源码：`/data/workspace/openjdk-cut-new/`
- 源码分析文档：`/data/workspace/openjdk-cut-new/new-jvm-md/`

## 实现阶段

### 阶段 1：最小可运行 JVM ⬜
- `.class` 文件解析（ClassFile 结构）
- 基础字节码解释器（switch-case，覆盖核心指令集）
- 栈帧管理（局部变量表 + 操作数栈）
- 目标：跑通 Hello World

### 阶段 2：内存管理 ⬜
- 堆内存分配（Region 设计参考 G1）
- 对象头（MarkWord + Klass 指针，参考 OpenJDK oop 设计）
- 简单标记-清除 GC

### 阶段 3：类加载体系 ⬜
- ClassFile 解析（参考 `ClassFileParser`）
- Klass / InstanceKlass 体系（参考 OpenJDK instanceKlass.hpp）
- 方法解析（vtable / itable）

### 阶段 4：运行时支持 ⬜
- 线程模型（参考 `JavaThread`）
- 同步机制（参考 `ObjectMonitor`）
- 异常处理

### 阶段 5：优化（可选）⬜
- 简单 JIT（模板解释器思路）
- G1 风格 GC

## 构建

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

## 目录结构

```
my_jvm/
├── src/
│   ├── classfile/      # .class 文件解析
│   ├── interpreter/    # 字节码解释器
│   ├── runtime/        # 运行时（线程、栈帧）
│   ├── memory/         # 内存管理与 GC
│   ├── oops/           # 对象体系（Klass / InstanceKlass / oop）
│   └── main.cpp        # 入口
├── test/               # 测试用 .class 文件
├── CMakeLists.txt
└── README.md
```
