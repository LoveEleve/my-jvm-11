# Mini JVM 手写路线图 —— 基于 HotSpot 真实启动流程

> **核心思路**：严格对照 `java MyClass` 的真实执行链，从 `main()` → `JLI_Launch()` → `JavaMain()` → `Threads::create_vm()` → `LoadMainClass()` → `CallStaticVoidMethod()` 的每一步，逐步实现对应的简化版。
>
> **最终目标**：`./mini_jvm -cp test HelloWorld` —— 像真正的 JVM 一样启动、加载、执行。

---

## 当前已有资产盘点（Phase 1-8 积累）

在重构路线之前，先确认**已有的可复用模块**：

| 模块 | 文件 | 状态 | HotSpot 对应 |
|------|------|------|-------------|
| 类型系统 | `globalDefinitions.hpp` | ✅ 完善 | `globalDefinitions.hpp` |
| 断言/调试 | `debug.hpp` | ✅ 完善 | `debug.hpp` |
| 内存分配基类 | `allocation.hpp/cpp` | ✅ 基本 | `allocation.hpp` |
| Class 文件解析器 | `classFileParser.hpp/cpp` + `classFileStream.hpp` | ✅ 完善 | `classFileParser.hpp/cpp` |
| 常量池 | `constantPool.hpp/cpp` | ✅ 完善 | `constantPool.hpp/cpp` |
| OOP-Klass 模型 | `klass.hpp`, `instanceKlass.hpp/cpp`, `oop.hpp` | ✅ 完善 | 对应 HotSpot |
| markOop | `markOop.hpp` | ✅ 完善 | `markOop.hpp` |
| Method | `method.hpp`, `constMethod.hpp` | ✅ 完善 | `method.hpp` |
| 数组支持 | `arrayKlass.hpp`, `typeArrayKlass.hpp/cpp`, `arrayOop.hpp`, `typeArrayOop.hpp` | ✅ 完善 | 对应 HotSpot |
| Java 堆 | `javaHeap.hpp/cpp` | ✅ bump-pointer | `collectedHeap.hpp` |
| 字节码定义 | `bytecodes.hpp` | ✅ 完善 | `bytecodes.hpp` |
| 解释器 | `bytecodeInterpreter.hpp/cpp` | ✅ switch-case | `bytecodeInterpreter.cpp` |
| 栈帧 | `frame.hpp` | ✅ 完善 | `frame.hpp` |
| JavaThread | `javaThread.hpp` | ✅ 基本 | `thread.hpp` |

**关键缺失**（对照 HotSpot 启动链）：

| 缺失组件 | HotSpot 对应 | 重要性 |
|----------|-------------|--------|
| `main()` 入口 + 参数解析 | `main.c` + `java.c` + `Arguments::parse()` | ★★★ |
| `Threads::create_vm()` 统一启动流程 | `thread.cpp:3876` | ★★★ |
| `vm_init_globals()` / `init_globals()` | `init.cpp` | ★★★ |
| `Universe` (全局单例) | `universe.hpp/cpp` | ★★★ |
| `SystemDictionary` (类字典) | `systemDictionary.hpp/cpp` | ★★★ |
| `ClassLoader` (从文件系统定位 .class) | `classLoader.hpp/cpp` | ★★★ |
| `JavaCalls::call()` (C++→Java 桥梁) | `javaCalls.hpp/cpp` | ★★★ |
| 类的链接 `link_class()` | `instanceKlass.cpp:711` | ★★ |
| 类的初始化 `initialize()` / `<clinit>` | `instanceKlass.cpp:892` | ★★ |
| `VMThread` | `vmThread.hpp/cpp` | ★ (可推迟) |
| 异常对象创建 | `universe_post_init()` | ★ |
| `Symbol` / `SymbolTable` | `symbol.hpp`, `symbolTable.hpp` | ★ (当前用 `char*` 替代) |

---

## 路线图总览

```
══════════════════════════════════════════════════════════════════
                     Mini JVM 开发路线图
              对照 HotSpot: java MyClass 完整执行链
══════════════════════════════════════════════════════════════════

┌───────────────────────────────────────────────────────────────┐
│  Phase 9: main() 入口 + 参数解析                              │
│  对照: main.c → JLI_Launch() → Arguments::parse()             │
│  目标: ./mini_jvm -cp test HelloWorld                         │
│        能解析 -cp 和类名参数                                   │
└───────────────────────────────────────┬───────────────────────┘
                                        │
                                        ▼
┌───────────────────────────────────────────────────────────────┐
│  Phase 10: Threads::create_vm() 统一启动框架                   │
│  对照: thread.cpp:3876 → vm_init_globals() + init_globals()    │
│  目标: 建立 VM 初始化的统一入口和流程骨架                       │
└───────────────────────────────────────┬───────────────────────┘
                                        │
                                        ▼
┌───────────────────────────────────────────────────────────────┐
│  Phase 11: Universe + 堆初始化                                │
│  对照: universe_init() → initialize_heap()                     │
│        universe2_init() → genesis() → 基本类型 Klass           │
│  目标: Universe 全局单例，集中管理堆 + 基本类型                 │
└───────────────────────────────────────┬───────────────────────┘
                                        │
                                        ▼
┌───────────────────────────────────────────────────────────────┐
│  Phase 12: SystemDictionary + ClassLoader                      │
│  对照: ClassLoader::load_class() (从 classpath 定位 .class)    │
│        SystemDictionary::resolve_or_null() (查找/缓存 Klass)   │
│  目标: 给定类名 → 自动找到 .class → 解析 → 缓存               │
└───────────────────────────────────────┬───────────────────────┘
                                        │
                                        ▼
┌───────────────────────────────────────────────────────────────┐
│  Phase 13: 类的链接与初始化                                    │
│  对照: InstanceKlass::link_class_impl() → link_methods()       │
│        InstanceKlass::initialize_impl() → <clinit>             │
│  目标: 自动链接 + 执行静态初始化器                              │
└───────────────────────────────────────┬───────────────────────┘
                                        │
                                        ▼
┌───────────────────────────────────────────────────────────────┐
│  Phase 14: JavaCalls + JavaMain 执行流                         │
│  对照: JavaMain() → LoadMainClass() → CallStaticVoidMethod()   │
│        JavaCalls::call() → call_helper() → 解释器              │
│  目标: 统一的方法调用框架 + main 方法执行                       │
└───────────────────────────────────────┬───────────────────────┘
                                        │
                                        ▼
┌───────────────────────────────────────────────────────────────┐
│  Phase 15: 异常处理框架                                        │
│  对照: universe_post_init() 预分配异常对象                      │
│        athrow / exception_table / catch                         │
│  目标: try-catch-finally 基本工作                               │
└───────────────────────────────────────┬───────────────────────┘
                                        │
                                        ▼
┌───────────────────────────────────────────────────────────────┐
│  Phase 16: 继承与多态 + 接口                                   │
│  对照: vtable/itable 初始化、invokevirtual 正式分派             │
│        invokeinterface、checkcast、instanceof                   │
│  目标: 真正的虚方法分派 + 接口方法调用                          │
└───────────────────────────────────────┬───────────────────────┘
                                        │
                                        ▼
┌───────────────────────────────────────────────────────────────┐
│  Phase 17: 字符串 + System.out.println                         │
│  对照: StringTable、java.lang.String 简化实现                   │
│        println 内建桩 → 真实调用链                              │
│  目标: System.out.println("Hello World") 真正工作              │
└───────────────────────────────────────┬───────────────────────┘
                                        │
                                        ▼
┌───────────────────────────────────────────────────────────────┐
│  Phase 18+: 高级特性（后续规划）                               │
│  - ObjArrayKlass（对象数组）                                    │
│  - GC 基础（标记-清除 → G1 Region 模型）                       │
│  - 多线程 + synchronized                                       │
│  - JIT 编译器基础                                              │
└───────────────────────────────────────────────────────────────┘
```

---

## Phase 9: main() 入口 + 参数解析

### 对照 HotSpot

```
main()                          [main.c:97]
  └─ JLI_Launch()               [java.c:273]
       ├─ ParseArguments()      [java.c:414]
       └─ JVMInit()             [java.c:435]
```

HotSpot 中：
- `main.c:97`: 程序入口，处理环境变量 (`JDK_JAVA_OPTIONS`)、`@argfile` 参数文件
- `java.c:414`: `ParseArguments()` 解析 `-cp`、`-classpath`、`-Xms`、`-Xmx`、`-XX:` 等参数
- `java.c:486`: `JavaMain()` 从解析后的参数获取 `mode`（类模式/JAR模式）和 `what`（主类名）

### 要做的事

**新增文件**：
- `src/share/runtime/arguments.hpp/cpp` — 对照 HotSpot `src/hotspot/share/runtime/arguments.hpp`

**修改文件**：
- `test/main.cpp` → 改为 `src/main.cpp`，成为真正的程序入口

### 实现内容

```cpp
// src/share/runtime/arguments.hpp
// 对照 HotSpot: Arguments::parse()

class Arguments {
public:
    // 解析命令行参数
    // 对照: Arguments::parse() [arguments.cpp:4261]
    static bool parse(int argc, char** argv);

    // 参数值
    static const char* classpath();        // -cp / -classpath 的值
    static const char* main_class_name();  // 主类名
    static size_t      heap_size();        // -Xmx（默认 256MB）

    // 内部存储
private:
    static const char* _classpath;
    static const char* _main_class_name;
    static size_t      _heap_size;
};
```

```cpp
// src/main.cpp — 真正的入口
// 对照 HotSpot: main.c:97 → JavaMain() [java.c:486]

int main(int argc, char** argv) {
    // 1. 解析参数（对照 ParseArguments）
    if (!Arguments::parse(argc, argv)) {
        print_usage();
        return 1;
    }

    // 2. 创建 VM（对照 InitializeJVM → Threads::create_vm）
    if (!VM::create_vm()) {
        return 1;
    }

    // 3. 加载主类（对照 LoadMainClass）
    // 4. 查找 main 方法（对照 GetStaticMethodID）
    // 5. 调用 main 方法（对照 CallStaticVoidMethod）
    // 6. 销毁 VM（对照 DestroyJavaVM）
    VM::destroy_vm();
    return 0;
}
```

### 验收标准

```bash
./mini_jvm -cp test HelloWorld
# → 解析出: classpath="test", main_class="HelloWorld"
# → 后续 Phase 真正执行
```

### HotSpot 源码参考

| 文件 | 行号 | 函数 |
|------|------|------|
| `main.c` | 97 | `main()` |
| `java.c` | 273 | `JLI_Launch()` |
| `java.c` | 414 | `ParseArguments()` |
| `java.c` | 486 | `JavaMain()` |
| `arguments.cpp` | 4261 | `Arguments::parse()` |

---

## Phase 10: Threads::create_vm() 统一启动框架

### 对照 HotSpot

```
JNI_CreateJavaVM()                      [jni.cpp:4108]
  └─ Threads::create_vm()               [thread.cpp:3876]
       ├─ vm_init_globals()             [init.cpp:90]
       │   ├─ basic_types_init()
       │   ├─ mutex_init()
       │   ├─ chunkpool_init()
       │   └─ perfMemory_init()
       │
       ├─ new JavaThread() + 附加到 OS 线程   [thread.cpp:4018]
       │
       ├─ init_globals()                [init.cpp:104]
       │   ├─ bytecodes_init()
       │   ├─ universe_init()           ← Phase 11
       │   ├─ interpreter_init()
       │   ├─ universe2_init()
       │   ├─ universe_post_init()
       │   └─ ...
       │
       └─ initialize_java_lang_classes() ← Phase 14
```

### 要做的事

**新增文件**：
- `src/share/runtime/vm.hpp/cpp` — 对照 `Threads::create_vm()` + `init.cpp`
- `src/share/runtime/init.hpp/cpp` — 对照 `init.cpp` 的 `vm_init_globals()` + `init_globals()`

### 实现内容

```cpp
// src/share/runtime/vm.hpp
// 统一 VM 生命周期管理
// 对照 HotSpot: Threads::create_vm() [thread.cpp:3876]

class VM {
public:
    // 创建 VM — 对照 Threads::create_vm()
    static bool create_vm();

    // 销毁 VM — 对照 Threads::destroy_vm()
    static void destroy_vm();

    // VM 状态
    static bool is_initialized() { return _initialized; }

    // 全局访问
    static JavaThread* main_thread() { return _main_thread; }

private:
    static bool _initialized;
    static JavaThread* _main_thread;
};
```

```cpp
// src/share/runtime/init.cpp
// 对照 HotSpot: init.cpp

// 阶段 1：基础设施
void vm_init_globals() {
    // 对照 init.cpp:90-98
    basic_types_init();       // 验证类型大小
    // mutex_init();          // 单线程暂不需要
    // perfMemory_init();     // 暂不需要
}

// 阶段 2：核心模块
jint init_globals() {
    // 对照 init.cpp:104-168
    bytecodes_init();         // 字节码表初始化（已有，需整合）
    universe_init();          // Phase 11: 堆 + 基本类型
    interpreter_init();       // 解释器初始化
    universe2_init();         // Phase 11: genesis — 创建基本类型 Klass
    universe_post_init();     // Phase 15: 预分配异常对象
    return JNI_OK;
}
```

### `create_vm()` 流程

```cpp
bool VM::create_vm() {
    // ═══ 阶段 1: 环境初始化 ═══
    // 对照 thread.cpp:3879-3948

    // ═══ 阶段 2: vm_init_globals() ═══
    // 对照 thread.cpp:4002
    vm_init_globals();

    // ═══ 阶段 3: 创建主线程 ═══
    // 对照 thread.cpp:4018-4055
    _main_thread = new JavaThread("main");
    _main_thread->set_thread_state(_thread_in_vm);

    // ═══ 阶段 4: init_globals() ═══
    // 对照 thread.cpp:4060
    if (init_globals() != JNI_OK) {
        return false;
    }

    _initialized = true;
    return true;
}
```

### 关键设计决策

| 决策点 | HotSpot 做法 | Mini JVM 简化方案 |
|--------|-------------|-----------------|
| TLS 线程存储 | `ThreadLocalStorage::init()` + `pthread_key_create` | 全局指针 `_main_thread` |
| 互斥锁初始化 | `mutex_init()` 创建 60+ 全局锁 | 单线程，不需要 |
| OS 初始化 | `os::init()` → 系统调用获取 CPU/内存/页大小 | 仅 `sysconf` 获取页大小 |
| Safepoint | `SafepointMechanism::initialize()` → mmap polling page | 单线程不需要 |
| VMThread | `VMThread::create()` + `pthread_create` | 不创建，单线程 |

### 验收标准

```bash
./mini_jvm -cp test HelloWorld
# 输出:
# [VM] vm_init_globals: basic_types_init done
# [VM] init_globals: bytecodes_init done
# [VM] init_globals: universe_init done
# [VM] init_globals: interpreter_init done
# [VM] VM created successfully
# ...
```

### HotSpot 源码参考

| 文件 | 行号 | 函数 |
|------|------|------|
| `thread.cpp` | 3876 | `Threads::create_vm()` |
| `init.cpp` | 90 | `vm_init_globals()` |
| `init.cpp` | 104 | `init_globals()` |
| `jni.cpp` | 3949 | `JNI_CreateJavaVM_inner()` |

---

## Phase 11: Universe + 堆初始化

### 对照 HotSpot

```
init_globals()
  └─ universe_init()                 [universe.cpp:681]
       ├─ Universe::initialize_heap() ← 创建 CollectedHeap + mmap 分配堆
       ├─ Metaspace::global_initialize()
       ├─ SymbolTable::create_table()
       └─ StringTable::create_table()

  └─ universe2_init()                [universe.cpp:1200]
       └─ Universe::genesis()
            ├─ 创建 8 种基本类型 TypeArrayKlass
            │   (bool/char/float/double/byte/short/int/long)
            ├─ vmSymbols::initialize()
            └─ SystemDictionary::initialize()
```

### 要做的事

**新增文件**：
- `src/share/memory/universe.hpp/cpp` — 对照 `universe.hpp/cpp`

**改造已有文件**：
- `javaHeap.hpp/cpp` — 由 Universe 统一管理，不再外部直接调用

### 实现内容

```cpp
// src/share/memory/universe.hpp
// 对照 HotSpot: src/hotspot/share/memory/universe.hpp
//
// Universe 是 JVM 的 "宇宙" — 包含所有全局状态：
//   - _collectedHeap: Java 堆
//   - _typeArrayKlassObjs[]: 8 种基本类型数组的 Klass
//   - _main_thread_group: main 线程组
//   - 预分配的异常对象（OOM/NPE/ArithmeticException 等）
//
// HotSpot 中 Universe 是纯静态类（全部 static 成员）

class Universe {
public:
    // ══════ 初始化入口 ══════

    // 对照 universe_init() [universe.cpp:681]
    // 创建堆 + SymbolTable + StringTable
    static jint initialize();

    // 对照 universe2_init() → genesis() [universe.cpp:1200]
    // 创建基本类型 TypeArrayKlass
    static void genesis();

    // 对照 universe_post_init() [universe.cpp:1210]
    // 预分配异常对象
    static bool post_initialize();

    // ══════ 全局访问 ══════

    static JavaHeap*      heap()              { return _heap; }
    static TypeArrayKlass* boolArrayKlass()   { return _typeArrayKlassObjs[T_BOOLEAN]; }
    static TypeArrayKlass* byteArrayKlass()   { return _typeArrayKlassObjs[T_BYTE]; }
    static TypeArrayKlass* charArrayKlass()   { return _typeArrayKlassObjs[T_CHAR]; }
    static TypeArrayKlass* intArrayKlass()    { return _typeArrayKlassObjs[T_INT]; }
    static TypeArrayKlass* longArrayKlass()   { return _typeArrayKlassObjs[T_LONG]; }
    static TypeArrayKlass* floatArrayKlass()  { return _typeArrayKlassObjs[T_FLOAT]; }
    static TypeArrayKlass* doubleArrayKlass() { return _typeArrayKlassObjs[T_DOUBLE]; }
    static TypeArrayKlass* shortArrayKlass()  { return _typeArrayKlassObjs[T_SHORT]; }

    // 通过 BasicType 获取对应的 TypeArrayKlass
    static TypeArrayKlass* typeArrayKlass(BasicType t) {
        return _typeArrayKlassObjs[t];
    }

    // ══════ 清理 ══════
    static void destroy();

private:
    // 对照 HotSpot: universe.hpp 中的静态成员
    static JavaHeap*       _heap;
    static TypeArrayKlass* _typeArrayKlassObjs[T_CONFLICT + 1]; // 按 BasicType 索引
    static bool            _fully_initialized;

    // 对照 Universe::initialize_heap() [universe.cpp:694]
    static bool initialize_heap();
};
```

### genesis() 实现要点

对照 `Universe::genesis()` (`universe.cpp:1079-1194`)：

```cpp
void Universe::genesis() {
    // 对照 universe.cpp:1088-1099
    // 创建 8 种基本类型的 TypeArrayKlass
    _typeArrayKlassObjs[T_BOOLEAN] = TypeArrayKlass::create_klass(T_BOOLEAN, "bool");
    _typeArrayKlassObjs[T_CHAR]    = TypeArrayKlass::create_klass(T_CHAR, "char");
    _typeArrayKlassObjs[T_FLOAT]   = TypeArrayKlass::create_klass(T_FLOAT, "float");
    _typeArrayKlassObjs[T_DOUBLE]  = TypeArrayKlass::create_klass(T_DOUBLE, "double");
    _typeArrayKlassObjs[T_BYTE]    = TypeArrayKlass::create_klass(T_BYTE, "byte");
    _typeArrayKlassObjs[T_SHORT]   = TypeArrayKlass::create_klass(T_SHORT, "short");
    _typeArrayKlassObjs[T_INT]     = TypeArrayKlass::create_klass(T_INT, "int");
    _typeArrayKlassObjs[T_LONG]    = TypeArrayKlass::create_klass(T_LONG, "long");

    // 注意 HotSpot 中的顺序：先 bool, char, float, double, byte, short, int, long
    // 这个顺序很重要，因为后续代码依赖这个 index

    // 后续: SystemDictionary::initialize() → Phase 12
}
```

### 整合已有的 JavaHeap

当前 `JavaHeap` 是通过 `JavaHeap::initialize()` 静态方法创建的。改为由 `Universe::initialize_heap()` 统一管理：

```cpp
bool Universe::initialize_heap() {
    // 对照 universe.cpp:694-727
    // HotSpot: _collectedHeap = create_heap() → GCArguments::create_heap()
    // 我们简化为直接创建 bump-pointer 堆
    size_t heap_size = Arguments::heap_size(); // 从参数获取
    _heap = new JavaHeap(heap_size);
    return _heap != nullptr;
}
```

### 验收标准

- `Universe::initialize()` 成功创建堆
- `Universe::genesis()` 创建 8 种 TypeArrayKlass
- `Universe::typeArrayKlass(T_INT)` 返回正确的 Klass
- 已有的数组测试继续通过

### HotSpot 源码参考

| 文件 | 行号 | 函数 |
|------|------|------|
| `universe.cpp` | 681 | `universe_init()` |
| `universe.cpp` | 694 | `Universe::initialize_heap()` |
| `universe.cpp` | 1079 | `Universe::genesis()` |
| `universe.cpp` | 1200 | `universe2_init()` |
| `universe.cpp` | 1210 | `universe_post_init()` |

---

## Phase 12: SystemDictionary + ClassLoader

### 对照 HotSpot

这是**最关键的 Phase** —— 实现"给一个类名，自动找到 .class 文件并返回 InstanceKlass"。

```
SystemDictionary::resolve_or_null()         [systemDictionary.cpp:246]
  └─ resolve_instance_class_or_null()       [systemDictionary.cpp:631]
       ├─ dictionary->find()                 ← 快速路径：字典缓存
       └─ load_instance_class()             [systemDictionary.cpp:1403]
            └─ ClassLoader::load_class()     [classLoader.cpp:1434]
                 └─ 打开 .class 文件
                 └─ KlassFactory::create_from_stream()
                      └─ ClassFileParser → InstanceKlass
```

### 要做的事

**新增文件**：
- `src/share/classfile/systemDictionary.hpp/cpp` — 类字典（已加载类的缓存）
- `src/share/classfile/classLoader.hpp/cpp` — 从 classpath 定位 .class 文件

### 实现内容

#### ClassLoader — 从文件系统定位 .class

```cpp
// src/share/classfile/classLoader.hpp
// 对照 HotSpot: src/hotspot/share/classfile/classLoader.hpp
//
// HotSpot 的 ClassLoader 负责:
//   1. 维护 boot classpath 条目列表（_first_append_entry）
//   2. 从 classpath 搜索 .class 文件
//   3. 读取文件为 ClassFileStream
//
// 简化版：只支持目录形式的 classpath（不支持 jar/jimage）

class ClassLoader {
public:
    // 初始化 — 设置 classpath
    // 对照: classLoader_init1() [init.cpp:113]
    static void initialize();

    // 从 classpath 加载类
    // 对照: ClassLoader::load_class() [classLoader.cpp:1434]
    // 输入: "com/example/Hello" → 搜索 <classpath>/com/example/Hello.class
    // 输出: InstanceKlass* (已解析但未链接)
    static InstanceKlass* load_class(const char* class_name);

private:
    // classpath 条目列表
    static const char* _classpath;

    // 辅助：将类名转换为文件路径
    // "com/example/Hello" → "<classpath>/com/example/Hello.class"
    static char* class_name_to_file_path(const char* class_name);

    // 读取文件为 ClassFileStream
    static ClassFileStream* open_stream(const char* file_path);
};
```

#### SystemDictionary — 已加载类的缓存

```cpp
// src/share/classfile/systemDictionary.hpp
// 对照 HotSpot: src/hotspot/share/classfile/systemDictionary.hpp
//
// SystemDictionary 是类加载的核心调度中心：
//   - 维护 "类名 → InstanceKlass*" 的哈希表
//   - 先查缓存，未命中才触发加载
//   - 加载完成后注册到字典
//
// HotSpot 中使用 Dictionary（基于 Hashtable）存储
// 我们简化为 std::unordered_map

class SystemDictionary {
public:
    // 初始化
    // 对照: SystemDictionary::initialize() [systemDictionary.cpp 在 genesis() 中调用]
    static void initialize();

    // ★ 核心方法：解析类名 → InstanceKlass*
    // 对照: SystemDictionary::resolve_or_null() [systemDictionary.cpp:246]
    //
    // 流程:
    //   1. 查字典缓存 → 命中则返回
    //   2. ClassLoader::load_class() 从文件系统加载
    //   3. link_class() 链接
    //   4. 注册到字典缓存
    //   5. 返回
    static InstanceKlass* resolve_or_null(const char* class_name);

    // 查找已加载的类（不触发加载）
    // 对照: SystemDictionary::find() [systemDictionary.cpp]
    static InstanceKlass* find(const char* class_name);

    // 清理
    static void destroy();

private:
    // 类名 → InstanceKlass* 映射
    // HotSpot 用 Dictionary（Hashtable 子类），我们用 std::unordered_map
    static std::unordered_map<std::string, InstanceKlass*>* _dictionary;

    // 注册到字典
    // 对照: update_dictionary() [systemDictionary.cpp]
    static void add_to_dictionary(const char* class_name, InstanceKlass* klass);
};
```

### 关键流程：`resolve_or_null()` 实现

```cpp
InstanceKlass* SystemDictionary::resolve_or_null(const char* class_name) {
    // Step 1: 查字典缓存
    // 对照: dictionary->find(d_hash, name, protection_domain) [systemDictionary.cpp:653]
    InstanceKlass* k = find(class_name);
    if (k != nullptr) {
        return k;  // 已加载，直接返回
    }

    // Step 2: 从文件系统加载
    // 对照: load_instance_class() → ClassLoader::load_class() [systemDictionary.cpp:1405]
    k = ClassLoader::load_class(class_name);
    if (k == nullptr) {
        return nullptr;  // ClassNotFoundException
    }

    // Step 3: 链接（Phase 13）
    // 对照: k->link_class() → link_class_impl() [instanceKlass.cpp:711]
    // 暂时标记为 linked
    k->set_init_state(InstanceKlass::linked);

    // Step 4: 注册到字典
    // 对照: update_dictionary() [systemDictionary.cpp:1713]
    add_to_dictionary(class_name, k);

    return k;
}
```

### 验收标准

```bash
./mini_jvm -cp test HelloWorld
# → ClassLoader 自动找到 test/HelloWorld.class
# → 解析为 InstanceKlass
# → 缓存到 SystemDictionary
# → 第二次查找直接命中缓存
```

### HotSpot 源码参考

| 文件 | 行号 | 函数 |
|------|------|------|
| `systemDictionary.cpp` | 246 | `resolve_or_null()` |
| `systemDictionary.cpp` | 631 | `resolve_instance_class_or_null()` |
| `systemDictionary.cpp` | 1403 | `load_instance_class()` |
| `classLoader.cpp` | 1434 | `ClassLoader::load_class()` |
| `klassFactory.cpp` | 166 | `KlassFactory::create_from_stream()` |

---

## Phase 13: 类的链接与初始化

### 对照 HotSpot

```
InstanceKlass::link_class_impl()     [instanceKlass.cpp:711]
  ├─ 递归链接父类
  ├─ verify_code()                    字节码验证
  ├─ rewrite_class()                  字节码重写
  ├─ link_methods()                   为每个方法设置入口点
  │   └─ Method::link_method()        设置 interpreter entry
  ├─ initialize_vtable()
  ├─ initialize_itable()
  └─ set_init_state(linked)

InstanceKlass::initialize_impl()     [instanceKlass.cpp:892]
  ├─ 递归初始化父类
  ├─ call_class_initializer()        执行 <clinit>
  │   └─ JavaCalls::call()           → 解释器执行
  └─ set_init_state(fully_initialized)
```

### 要做的事

**改造已有文件**：
- `instanceKlass.hpp/cpp` — 添加 `link_class()` 和 `initialize()`

### 实现内容

```cpp
// 添加到 InstanceKlass

// 对照 HotSpot: InstanceKlass::link_class_impl() [instanceKlass.cpp:711]
bool link_class() {
    if (is_linked()) return true;  // 已链接

    // Step 1: 递归链接父类（当前简化为不处理继承）
    // 对照: instanceKlass.cpp:737 ik_super->link_class_impl()

    // Step 2: 验证（跳过，我们信任编译器）
    // 对照: instanceKlass.cpp:756 verify_code()

    // Step 3: 链接方法 — 确保每个方法可被调用
    // 对照: instanceKlass.cpp:776 link_methods()
    // 我们的简化版不需要设置机器码入口点

    // Step 4: 标记为已链接
    set_init_state(linked);
    return true;
}

// 对照 HotSpot: InstanceKlass::initialize_impl() [instanceKlass.cpp:892]
void initialize(JavaThread* thread) {
    if (is_initialized()) return;

    // Step 1: 确保已链接
    link_class();

    // Step 2: 设置为 being_initialized
    set_init_state(being_initialized);

    // Step 3: 递归初始化父类（Phase 16 完善）

    // Step 4: 执行 <clinit>
    // 对照: instanceKlass.cpp:988 call_class_initializer()
    Method* clinit = find_method("<clinit>", "()V");
    if (clinit != nullptr) {
        JavaValue result(T_VOID);
        // 通过解释器执行 <clinit>
        BytecodeInterpreter::execute(clinit, this, thread, &result);
    }

    // Step 5: 标记为完全初始化
    set_init_state(fully_initialized);
}
```

### 验收标准

```java
// test/Counter.java
public class Counter {
    static int count = 42;  // <clinit> 会设置这个值
    public static int getCount() { return count; }
}
```

```bash
./mini_jvm -cp test Counter
# → load Counter.class
# → link_class() 成功
# → initialize() 执行 <clinit> 设置 count = 42
# → 可以通过 getstatic 读取到 42
```

### HotSpot 源码参考

| 文件 | 行号 | 函数 |
|------|------|------|
| `instanceKlass.cpp` | 711 | `link_class_impl()` |
| `instanceKlass.cpp` | 776 | `link_methods()` |
| `instanceKlass.cpp` | 892 | `initialize_impl()` |
| `instanceKlass.cpp` | 988 | `call_class_initializer()` |

---

## Phase 14: JavaCalls + JavaMain 执行流

### 对照 HotSpot

```
JavaMain()                                    [java.c:486]
  ├─ LoadMainClass(env, mode, what)           [java.c:604]
  │    └─ Class.forName(what)
  │         └─ SystemDictionary::resolve_or_null()
  │
  ├─ GetStaticMethodID("main", "([Ljava/lang/String;)V")  [java.c:641]
  │
  └─ CallStaticVoidMethod(mainClass, mainID, mainArgs)     [java.c:647]
       └─ jni_CallStaticVoidMethod()          [jni.cpp:1984]
            └─ jni_invoke_static()            [jni.cpp:1108]
                 └─ JavaCalls::call()         [javaCalls.cpp:339]
                      └─ call_helper()        [javaCalls.cpp:348]
                           ├─ entry_point = method->from_interpreted_entry()
                           └─ StubRoutines::call_stub()() → 解释器
```

### 要做的事

**新增文件**：
- `src/share/runtime/javaCalls.hpp/cpp` — 统一的 C++→Java 方法调用框架

**改造文件**：
- `src/main.cpp` — 完成完整的 JavaMain 流程

### 实现内容

```cpp
// src/share/runtime/javaCalls.hpp
// 对照 HotSpot: src/hotspot/share/runtime/javaCalls.hpp
//
// JavaCalls 是从 C++ 代码调用 Java 方法的统一入口。
// HotSpot 中：
//   JavaCalls::call() → call_helper() → StubRoutines::call_stub()()
//   call_stub 是一段预生成的机器码，负责设置栈帧并跳转到解释器入口。
//
// 我们的简化版：直接调用 BytecodeInterpreter::execute()

class JavaCalls {
public:
    // 调用静态方法
    // 对照: JavaCalls::call_static() [javaCalls.cpp:305]
    static void call_static(JavaValue* result,
                           InstanceKlass* klass,
                           Method* method,
                           JavaThread* thread,
                           intptr_t* args = nullptr,
                           int args_count = 0);

    // 调用实例方法
    // 对照: JavaCalls::call_virtual() [javaCalls.cpp:269]
    static void call_virtual(JavaValue* result,
                            InstanceKlass* klass,
                            Method* method,
                            JavaThread* thread,
                            intptr_t* args,
                            int args_count);

    // 通用调用
    // 对照: JavaCalls::call() [javaCalls.cpp:339]
    static void call(JavaValue* result,
                    InstanceKlass* klass,
                    Method* method,
                    JavaThread* thread,
                    intptr_t* args = nullptr,
                    int args_count = 0);
};
```

### 完整的 main() 入口

```cpp
// src/main.cpp — 完整版
// 对照 HotSpot: main.c:97 → JavaMain() [java.c:486]

int main(int argc, char** argv) {
    // ═══ Step 1: 解析参数 ═══
    // 对照: ParseArguments() [java.c:414]
    if (!Arguments::parse(argc, argv)) {
        fprintf(stderr, "Usage: mini_jvm -cp <classpath> <mainclass>\n");
        return 1;
    }

    // ═══ Step 2: 创建 VM ═══
    // 对照: InitializeJVM() → Threads::create_vm() [java.c:1603]
    if (!VM::create_vm()) {
        fprintf(stderr, "Error: Could not create the Java Virtual Machine.\n");
        return 1;
    }

    JavaThread* thread = VM::main_thread();

    // ═══ Step 3: 加载主类 ═══
    // 对照: LoadMainClass() [java.c:604]
    const char* main_class_name = Arguments::main_class_name();
    InstanceKlass* main_klass = SystemDictionary::resolve_or_null(main_class_name);
    if (main_klass == nullptr) {
        fprintf(stderr, "Error: Could not find or load main class %s\n", main_class_name);
        VM::destroy_vm();
        return 1;
    }

    // ═══ Step 4: 初始化主类（执行 <clinit>）═══
    // 对照: ensure_initialized() — 在 CallStaticVoidMethod 路径中触发
    main_klass->initialize(thread);

    // ═══ Step 5: 查找 main 方法 ═══
    // 对照: GetStaticMethodID("main", "([Ljava/lang/String;)V") [java.c:641]
    Method* main_method = main_klass->find_method("main", "([Ljava/lang/String;)V");
    if (main_method == nullptr) {
        fprintf(stderr, "Error: Main method not found in class %s\n", main_class_name);
        VM::destroy_vm();
        return 1;
    }

    // ═══ Step 6: 调用 main 方法 ═══
    // 对照: CallStaticVoidMethod() [java.c:647]
    //    → jni_invoke_static() → JavaCalls::call()
    JavaValue result(T_VOID);
    JavaCalls::call_static(&result, main_klass, main_method, thread);

    if (thread->has_pending_exception()) {
        fprintf(stderr, "Exception in thread \"main\": %s\n",
                thread->exception_message());
    }

    // ═══ Step 7: 销毁 VM ═══
    // 对照: LEAVE() → DestroyJavaVM() [java.c:651]
    VM::destroy_vm();
    return 0;
}
```

### 验收标准 🎯 ★ 里程碑 ★

```bash
./mini_jvm -cp test HelloWorld
# 输出:
# [VM] Initializing...
# [VM] Universe::initialize: heap created (256MB)
# [VM] Universe::genesis: TypeArrayKlasses created
# [VM] ClassLoader: loading "HelloWorld" from test/HelloWorld.class
# [VM] SystemDictionary: registered "HelloWorld"
# 7                          ← ★ 真正执行 HelloWorld.class 的输出
# [VM] Shutdown complete
```

这是第一个**真正像 JVM 一样工作**的里程碑：
- 从命令行接收 `-cp` 和类名
- 自动定位 .class 文件
- 解析、链接、初始化
- 查找并执行 main 方法
- 正常退出

### HotSpot 源码参考

| 文件 | 行号 | 函数 |
|------|------|------|
| `java.c` | 486 | `JavaMain()` |
| `java.c` | 604 | `LoadMainClass()` |
| `java.c` | 641 | `GetStaticMethodID()` |
| `java.c` | 647 | `CallStaticVoidMethod()` |
| `javaCalls.cpp` | 339 | `JavaCalls::call()` |
| `javaCalls.cpp` | 348 | `call_helper()` |
| `jni.cpp` | 1108 | `jni_invoke_static()` |

---

## Phase 15: 异常处理框架

### 对照 HotSpot

```
universe_post_init()                [universe.cpp:1210]
  ├─ 预分配 OutOfMemoryError (6 种)
  ├─ 预分配 NullPointerException
  ├─ 预分配 ArithmeticException
  └─ 预分配 VirtualMachineError

字节码层面:
  athrow          → 抛出异常
  exception_table → 查找 catch handler
  monitorenter/monitorexit → synchronized 的 finally
```

### 要做的事

- 实现 `athrow` 字节码
- 实现异常表查找（在 `InterpreterFrame::run()` 中）
- 实现 `exception_table` 解析（在 ClassFileParser 中）
- 预分配关键异常对象

### 实现要点

```cpp
// 异常表条目（在 ClassFileParser 中解析 Code 属性时读取）
struct ExceptionTableEntry {
    u2 start_pc;       // try 块开始
    u2 end_pc;         // try 块结束
    u2 handler_pc;     // catch 处理器入口
    u2 catch_type;     // 常量池索引（0 = finally/catch-all）
};

// 在解释器中，遇到 athrow 或异常时：
// 1. 在当前方法的异常表中查找匹配的 handler
// 2. 如果找到 → 跳转到 handler_pc 继续执行
// 3. 如果没找到 → 弹出当前帧，在调用者帧中继续查找（异常传播）
```

### 验收标准

```java
public class ExceptionTest {
    public static void main(String[] args) {
        try {
            int x = 1 / 0;
        } catch (ArithmeticException e) {
            // 被捕获
        }
    }
}
```

### HotSpot 源码参考

| 文件 | 行号 | 函数 |
|------|------|------|
| `universe.cpp` | 1210 | `universe_post_init()` |
| `universe.cpp` | 1231 | 预分配 OOM |
| `universe.cpp` | 1248 | 预分配 NPE |
| `bytecodeInterpreter.cpp` | - | `athrow` 处理 |

---

## Phase 16: 继承与多态 + 接口

### 对照 HotSpot

```
ClassFileParser::fill_instance_klass()
  └─ vtable 大小计算: klassVtable::compute_vtable_size_and_num_mirandas()
  └─ itable 大小计算: klassItable::compute_itable_size()

link_class_impl()
  └─ klassVtable::initialize_vtable()    [klassVtable.cpp]
  └─ klassItable::initialize_itable()    [klassVtable.cpp]

invokevirtual 字节码:
  → 取 receiver 对象
  → receiver->klass() → vtable[vtable_index] → 目标方法

invokeinterface 字节码:
  → 取 receiver 对象
  → itable 查找 → 目标方法
```

### 要做的事

- 实现 vtable（虚方法表）
- 实现 `invokevirtual` 的真正虚分派（当前是静态查找）
- 实现 `invokeinterface`
- 实现 `checkcast` 和 `instanceof`
- 支持类继承（加载类时递归解析父类）

### 验收标准

```java
public class Animal { public int speak() { return 1; } }
public class Dog extends Animal { public int speak() { return 2; } }
// Animal a = new Dog(); a.speak() → 2（虚分派）
```

---

## Phase 17: 字符串 + System.out.println

### 对照 HotSpot

```
StringTable::create_table()            [universe_init]
java.lang.String 内部结构:
  - value: byte[] (Compact Strings, JDK 9+)
  - coder: byte (LATIN1=0, UTF16=1)
  - hash:  int

System.out.println() 调用链:
  System.out → PrintStream 对象 → PrintStream.println(String)
  → 最终调用 native: FileOutputStream.writeBytes() → write(2) 系统调用
```

### 要做的事

- 实现简化版 `java.lang.String`（内部用 `char[]` / `byte[]`）
- `ldc` 支持字符串常量
- 实现 `System.out.println` 的内建桩（识别特定调用链，直接 `printf`）
- 实现 `ObjArrayKlass`（对象数组，`String[]` 需要）

### 验收标准 🎯 ★★ 最终里程碑 ★★

```java
public class HelloWorld {
    public static void main(String[] args) {
        System.out.println("Hello, World!");
    }
}
```

```bash
./mini_jvm -cp test HelloWorld
Hello, World!
```

---

## Phase 18+: 高级特性（后续规划）

| Phase | 内容 | 对照 HotSpot |
|-------|------|-------------|
| 18 | ObjArrayKlass（对象数组 `new Object[10]`） | `objArrayKlass.hpp/cpp` |
| 19 | GC 基础 — 标记-清除 | `collectedHeap.hpp` |
| 20 | G1 Region 模型 | `g1CollectedHeap.hpp`, `heapRegion.hpp` |
| 21 | 多线程 + `synchronized` | `objectMonitor.hpp`, `mutex.hpp` |
| 22 | JIT 编译器基础 | `compileBroker.hpp`, `c1_LIR.hpp` |

---

## 关键设计原则

### 1. 忠实对照 HotSpot 架构

每个新增的类/文件都必须标注对应的 HotSpot 源文件和行号。命名尽量一致。

### 2. 渐进式复杂度

```
Phase 9-10: 框架骨架（能启动，输出日志）
Phase 11-12: 核心管线（能加载类）
Phase 13-14: ★ 里程碑 — 能执行 .class 文件 ★
Phase 15-16: 完善语义（异常、多态）
Phase 17:    ★★ 最终里程碑 — Hello World ★★
```

### 3. 每个 Phase 必须可测试

每个 Phase 完成后都有明确的验收标准。test/main.cpp 中的单元测试继续保留作为回归测试。

### 4. 保持已有代码工作

Phase 1-8 的所有 35 个测试在重构后必须继续通过。新的 `main()` 入口与旧的测试代码可以通过编译选项切换。

---

## 文件结构规划

```
src/
├── main.cpp                              ← Phase 9: 新的真正入口
├── share/
│   ├── classfile/
│   │   ├── classFileParser.hpp/cpp       ← 已有
│   │   ├── classFileStream.hpp           ← 已有
│   │   ├── classLoader.hpp/cpp           ← Phase 12: NEW
│   │   └── systemDictionary.hpp/cpp      ← Phase 12: NEW
│   ├── gc/shared/
│   │   └── javaHeap.hpp/cpp              ← 已有，Phase 11 改为 Universe 管理
│   ├── interpreter/
│   │   ├── bytecodeInterpreter.hpp/cpp   ← 已有
│   │   └── bytecodes.hpp                 ← 已有
│   ├── memory/
│   │   ├── allocation.hpp/cpp            ← 已有
│   │   └── universe.hpp/cpp              ← Phase 11: NEW
│   ├── oops/
│   │   ├── (全部已有)
│   │   └── ...
│   ├── runtime/
│   │   ├── arguments.hpp/cpp             ← Phase 9: NEW
│   │   ├── frame.hpp                     ← 已有
│   │   ├── init.hpp/cpp                  ← Phase 10: NEW
│   │   ├── javaCalls.hpp/cpp             ← Phase 14: NEW
│   │   ├── javaThread.hpp                ← 已有
│   │   └── vm.hpp/cpp                    ← Phase 10: NEW
│   └── utilities/
│       ├── (全部已有)
│       └── ...
└── test/
    ├── main.cpp                          ← 保留为回归测试
    ├── HelloWorld.java
    └── HelloWorld.class
```

---

## 执行优先级

**立即开始**: Phase 9 → Phase 10 → Phase 11 → Phase 12 → Phase 13 → Phase 14

Phase 9-14 形成完整的 "从命令行到执行字节码" 链路。这 6 个 Phase 紧密耦合，应一气呵成。

**预计工作量**：

| Phase | 预计时间 | 核心工作 |
|-------|---------|---------|
| Phase 9 | 1-2 小时 | 参数解析 + main 入口 |
| Phase 10 | 2-3 小时 | VM 启动框架 + init_globals |
| Phase 11 | 2-3 小时 | Universe + genesis |
| Phase 12 | 3-4 小时 | ClassLoader + SystemDictionary |
| Phase 13 | 2-3 小时 | link + initialize |
| Phase 14 | 2-3 小时 | JavaCalls + JavaMain 完整流程 |
| **合计** | **12-18 小时** | **第一个真正的 JVM 里程碑** |
