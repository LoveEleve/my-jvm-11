# Phase 4：字节码解释器（BytecodeInterpreter）

## 概述

Phase 4 实现了一个完整的 C++ switch-case 字节码解释器，可以解析并执行从 `.class` 文件加载的 Java 方法。这是 Mini JVM 从"数据结构"到"能跑代码"的质变。

**终极验证**：解析 `HelloWorld.class` → 找到 `main()` → 执行字节码 → 打印 `7` 到 stdout。

## 新增文件

| 文件 | 大小 | 对应 HotSpot | 说明 |
|------|------|-------------|------|
| `src/share/interpreter/bytecodes.hpp` | 470 行 | `interpreter/bytecodes.hpp` | 完整 JVM 字节码枚举（203 个） |
| `src/share/runtime/javaThread.hpp` | 189 行 | `runtime/thread.hpp` | JavaThread + JavaFrameAnchor + JavaValue |
| `src/share/runtime/frame.hpp` | 305 行 | `runtime/frame.hpp` + `interpreter/bytecodeInterpreter.hpp` | InterpreterFrame（解释器栈帧） |
| `src/share/interpreter/bytecodeInterpreter.hpp` | 87 行 | `interpreter/bytecodeInterpreter.hpp` | 解释器类声明 |
| `src/share/interpreter/bytecodeInterpreter.cpp` | ~1000 行 | `interpreter/bytecodeInterpreter.cpp` | 核心执行引擎 |

## 架构设计

### 1. 执行模型

```
BytecodeInterpreter::execute(method, klass, thread, result, args)
  │
  ├── 创建 InterpreterFrame（局部变量表 + 操作数栈）
  ├── 设置参数到局部变量表
  └── 调用 run() 进入主循环
       │
       └── while(true) {
              读取当前字节码 (*bcp)
              switch(opcode) {
                case iadd: pop v2, pop v1, push v1+v2; break;
                case ireturn: pop ret, 设置 result; return;
                case invokestatic: invoke_method(递归); break;
                ...
              }
              推进 bcp
           }
```

### 2. InterpreterFrame 内存布局

```
┌────────────────────────────────────┐
│ _locals[0]   ← receiver/arg0      │  intptr_t 数组
│ _locals[1]                         │
│ ...                                │
│ _locals[max_locals-1]              │
├────────────────────────────────────┤
│ _stack[0]                          │  intptr_t 数组
│ _stack[1]                          │
│ ...                                │
│ _stack[_sp-1] ← 栈顶              │
│ _stack[_sp]   ← 下一个空位        │
│ ...                                │
│ _stack[max_stack-1]                │
├────────────────────────────────────┤
│ _bcp → 当前字节码位置              │
│ _method → Method*                  │
│ _constants → ConstantPool*         │
│ _caller → 调用者帧 (链表)         │
└────────────────────────────────────┘
```

### 3. 方法调用模型

```
main() 帧                add() 帧
┌──────────┐            ┌──────────┐
│locals: 3 │            │locals: 3 │
│stack:  3 │──invoke──→│stack:  2 │
│bcp: 11   │            │bcp: 0   │
│caller:nil│            │caller:──│──→ main帧
└──────────┘            └──────────┘
```

方法调用通过递归实现：
1. 从常量池解析 Methodref → 获取类名、方法名、签名
2. **目标类检查**：如果目标类不是当前 klass，跳过执行（桩处理）
3. 在当前 klass 中查找方法
4. 解析签名计算参数 slot 数
5. 从调用者栈弹出参数
6. 创建新 InterpreterFrame，参数复制到 locals
7. 递归调用 `run()`
8. 根据返回类型将结果压回调用者栈

### 4. 已实现的字节码

| 分类 | 字节码 |
|------|--------|
| 常量加载 | nop, aconst_null, iconst_m1~5, bipush, sipush, ldc(Integer/String) |
| 局部变量加载 | iload, iload_0~3, aload, aload_0~3 |
| 局部变量存储 | istore, istore_0~3, astore, astore_0~3 |
| 栈操作 | pop, dup, dup_x1, swap |
| 整数算术 | iadd, isub, imul, idiv, irem, ineg |
| 位运算 | iand, ior, ixor, ishl, ishr, iushr |
| 自增 | iinc |
| 类型转换 | i2l, i2b, i2c, i2s |
| 条件跳转 | ifeq/ifne/iflt/ifge/ifgt/ifle, if_icmpeq/ne/lt/ge/gt/le |
| 无条件跳转 | goto |
| 方法返回 | ireturn, lreturn, areturn, return |
| 方法调用 | invokestatic, invokespecial, invokevirtual |
| 字段访问 | getstatic(桩), putstatic(桩), getfield(桩), putfield(桩) |
| 对象创建 | new(桩) |

### 5. 桩实现说明

以下功能使用桩（stub）实现，后续 Phase 逐步完善：

| 功能 | 桩行为 | 后续完善 |
|------|--------|---------|
| `new` | 压入 0xDEAD0001 标记值 | Phase 5: 堆分配 |
| `getstatic` | System.out → 压入 0xDEAD0002 | Phase 7: 完整字段解析 |
| `putfield` | 弹出 value 和 objectref | Phase 5: 真正字段写入 |
| `getfield` | 弹出 objectref, 压入 0 | Phase 5: 真正字段读取 |
| `PrintStream.println(I)V` | `printf("%d\n", value)` | Phase 7: 标准库 |
| 外部类方法调用 | 弹出参数，跳过执行 | Phase 6: 类加载器 |

### 6. 关键类说明

#### Bytecodes (bytecodes.hpp)
- 完整 JVM 字节码枚举 (0x00~0xCA)
- `name(Code)` — 返回字节码名称字符串
- `length_for(Code)` — 返回指令字节长度

#### JavaThread (javaThread.hpp)
- 精简版 Java 线程，单线程设计
- 核心字段：线程状态、帧锚点、pending exception
- `JavaFrameAnchor` — Java/Native 帧边界
- `JavaValue` — 方法返回值的 tagged union

#### InterpreterFrame (frame.hpp)
- 独立的局部变量表 + 操作数栈（`intptr_t` 数组）
- 支持 int/long/float/double/oop 的 push/pop
- BCP 操作：读取操作数（u1/s1/u2/s2）、推进/跳转
- 帧链表：`_caller` 指向调用者帧

#### BytecodeInterpreter (bytecodeInterpreter.hpp/.cpp)
- 纯静态类，switch-case 派发
- `execute()` — 外部入口
- `run()` — 核心循环
- `invoke_method()` — 方法调用（invokestatic/invokespecial）
- `handle_invokevirtual()` — 虚方法调用（特殊处理 PrintStream.println）
- `handle_getstatic()` — 静态字段访问（特殊处理 System.out）
- `_trace_bytecodes` — 调试开关

## 修复的问题

### 1. oopDesc 前向声明缺失
`frame.hpp` 中使用了 `oopDesc*` 但没有声明。添加 `class oopDesc;` 前向声明。

### 2. constantTag 类型不匹配
`ConstantPool::tag_at()` 返回 `constantTag` 对象而非 `u1`。改用 `.value()` 方法获取 raw tag 值。

### 3. data_at() 方法缺失
`ConstantPool` 缺少 raw slot 访问方法。虽然添加了 `data_at()`，但最终改为使用高级 API：
- `unchecked_klass_ref_index_at()` — 获取 class_index
- `unchecked_name_and_type_ref_index_at()` — 获取 nat_index
- `name_ref_index_at()` / `signature_ref_index_at()` — 解析 NameAndType
- `klass_name_at()` — 获取类名字符串

### 4. 外部类方法无限递归
`HelloWorld.<init>` 中 `invokespecial Object.<init>()V` 在当前 klass 中找到了 HelloWorld 自己的 `<init>()V`，导致无限递归。修复：在查找方法前先比较目标类名，外部类方法直接弹出参数并跳过。

## 测试结果

```
========================================
  Mini JVM - Phase 4: Interpreter
========================================

=== Test: Bytes (Endian) ===               [PASS]
=== Test: ClassFileStream ===              [PASS]
=== Test: Mark Word ===                    [PASS]
=== Test: oopDesc Header ===               [PASS]
=== Test: AccessFlags ===                  [PASS]
=== Test: Metadata / Method ===            [PASS]
=== Test: Klass / InstanceKlass ===        [PASS]
=== Test: Full Pipeline ===                [PASS]
=== Test: Bytecodes ===                    [PASS]
=== Test: JavaThread ===                   [PASS]
=== Test: InterpreterFrame ===             [PASS]
=== Test: Interpreter Simple Add (3+4) === [PASS] → 结果: 7
=== Test: Interpreter With Args (10+25) == [PASS] → 结果: 35
=== Test: Interpreter Branch (abs) ===     [PASS] → abs(-42)=42, abs(99)=99
=== Test: Full Execution ===               [PASS] → 打印 "7"

All 14 tests passed!
```

### Valgrind 验证

```
HEAP SUMMARY:
    in use at exit: 0 bytes in 0 blocks
    total heap usage: 167 allocs, 167 frees, 92,775 bytes allocated
All heap blocks were freed -- no leaks are possible
ERROR SUMMARY: 0 errors from 0 contexts
```

## HelloWorld 完整执行流程

```java
// HelloWorld.java
public static void main(String[] args) {
    HelloWorld hw = new HelloWorld();    // new + dup + invokespecial <init>
    int result = hw.add(3, 4);          // aload_1 + iconst_3 + iconst_4 + invokevirtual add
    System.out.println(result);          // getstatic out + iload_2 + invokevirtual println
}
```

字节码执行 trace：
```
BB 00 03   new #3 (HelloWorld)         → push 0xDEAD0001 (stub)
59         dup                          → push copy
B7 00 04   invokespecial #4 (<init>)   → 递归执行 HelloWorld.<init>
  2A       aload_0                      → push this
  B7 00 01 invokespecial #1 (Object.<init>) → skipped (external class)
  2A       aload_0                      → push this
  03       iconst_0                     → push 0
  B5 00 02 putfield #2 (count)         → pop value + objectref (stub)
  B1       return                       → 返回
4C         astore_1                     → store hw to local[1]
2B         aload_1                      → push hw
06         iconst_3                     → push 3
07         iconst_4                     → push 4
B6 00 05   invokevirtual #5 (add)      → 递归执行 add(II)I
  1B       iload_1                      → push a (=3)
  1C       iload_2                      → push b (=4)
  60       iadd                         → push 7
  AC       ireturn                      → return 7
3D         istore_2                     → store 7 to local[2]
B2 00 06   getstatic #6 (System.out)   → push PrintStream (stub)
1C         iload_2                      → push 7
B6 00 07   invokevirtual #7 (println)  → printf("7\n") ← 实际输出！
B1         return                       → 完成
```

## 项目结构

```
my_jvm_11/
├── CMakeLists.txt
├── docs/
│   ├── Phase-01-Foundation.md
│   ├── Phase-02-ClassFileParser.md
│   ├── Phase-03-OOP-Klass.md
│   └── Phase-04-Interpreter.md          ← 本文档
├── src/share/
│   ├── utilities/
│   │   ├── globalDefinitions.hpp
│   │   ├── debug.hpp
│   │   ├── macros.hpp
│   │   ├── bytes.hpp
│   │   ├── accessFlags.hpp
│   │   └── constantTag.hpp
│   ├── memory/
│   │   └── allocation.hpp
│   ├── classfile/
│   │   ├── classFileStream.hpp
│   │   ├── classFileParser.hpp
│   │   └── classFileParser.cpp
│   ├── oops/
│   │   ├── oopsHierarchy.hpp
│   │   ├── markOop.hpp
│   │   ├── oop.hpp
│   │   ├── instanceOop.hpp
│   │   ├── metadata.hpp
│   │   ├── constMethod.hpp
│   │   ├── method.hpp
│   │   ├── klass.hpp
│   │   ├── instanceKlass.hpp
│   │   ├── instanceKlass.cpp
│   │   ├── constantPool.hpp
│   │   └── constantPool.cpp
│   ├── runtime/                          ← Phase 4 新增
│   │   ├── javaThread.hpp
│   │   └── frame.hpp
│   └── interpreter/                      ← Phase 4 新增
│       ├── bytecodes.hpp
│       ├── bytecodeInterpreter.hpp
│       └── bytecodeInterpreter.cpp
└── test/
    ├── main.cpp                          ← 14 个测试
    ├── HelloWorld.java
    └── HelloWorld.class
```

## 设计决策

| 决策 | 选择 | 原因 |
|------|------|------|
| 派发方式 | switch-case | 清晰易懂，适合学习（HotSpot 也有此模式） |
| 帧分配 | 堆分配 (CHeapObj) | 简单直接，后续可改栈分配 |
| 方法调用 | 递归 run() | 符合 HotSpot C++ 解释器的模型 |
| 外部类方法 | 桩处理 | 没有类加载器前无法执行外部代码 |
| PrintStream.println | printf 硬编码 | 最小可行的输出能力 |

## 后续 Phase 计划

| Phase | 内容 | 依赖 |
|-------|------|------|
| Phase 5 | 对象创建 + 堆分配 | 需要 Java 堆管理 |
| Phase 6 | 类加载器 | 支持加载多个类 |
| Phase 7 | Native 方法桥接 | 替换桩实现 |
| Phase 8 | G1 GC 基础 | Region 分配 + 标记 |
| Phase 9 | G1 GC 完整 | YGC + Mixed GC |
| Phase 10 | 完整 Hello World | 所有组件集成 |
