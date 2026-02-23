#include "classfile/classLoader.hpp"
#include "classfile/classFileStream.hpp"
#include "classfile/classFileParser.hpp"
#include "oops/instanceKlass.hpp"
#include "runtime/arguments.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>

// ============================================================================
// ClassLoader — Bootstrap 类加载器
// 对照 HotSpot: src/hotspot/share/classfile/classLoader.cpp
// ============================================================================

const char* ClassLoader::_classpath = ".";

void ClassLoader::initialize() {
    // 对照: classLoader_init1() [init.cpp:113]
    // HotSpot 中:
    //   1. 设置 boot classpath
    //   2. 初始化 class path 条目链表
    //   3. 初始化 jimage（模块镜像）
    // 我们简化为从 Arguments 获取 classpath
    _classpath = Arguments::classpath();

    fprintf(stderr, "[VM] ClassLoader::initialize: classpath=\"%s\"\n", _classpath);
}

InstanceKlass* ClassLoader::load_class(const char* class_name) {
    // 对照: ClassLoader::load_class() [classLoader.cpp:1434]
    //
    // HotSpot 中按优先级尝试三次加载:
    //   1. --patch-module 条目
    //   2. jimage（模块镜像）或 exploded build
    //   3. -Xbootclasspath/a 和 JVMTI 追加条目
    //
    // 找到 .class 文件后:
    //   ClassFileStream stream(...)
    //   KlassFactory::create_from_stream(stream, name, ...)
    //     → ClassFileParser parser(stream, ...)
    //     → parser.create_instance_klass()
    //
    // 我们简化为直接从 classpath 目录搜索

    // Step 1: 构造文件路径
    char* file_path = class_name_to_file_path(class_name);
    if (file_path == nullptr) {
        return nullptr;
    }

    // Step 2: 读取文件
    int file_length = 0;
    u1* buffer = read_file(file_path, &file_length);

    if (buffer == nullptr) {
        fprintf(stderr, "[VM] ClassLoader: could not find %s\n", file_path);
        free(file_path);
        return nullptr;
    }

    fprintf(stderr, "[VM] ClassLoader: loading \"%s\" from %s (%d bytes)\n",
            class_name, file_path, file_length);
    free(file_path);

    // Step 3: 创建 ClassFileStream
    // 对照: KlassFactory::create_from_stream() [klassFactory.cpp:166]
    ClassFileStream stream(buffer, file_length, class_name);

    // Step 4: 解析 .class 文件
    // 对照: ClassFileParser parser(stream, ...) → parser.parse_stream()
    ClassFileParser parser(&stream);
    parser.parse();

    // Step 5: 创建 InstanceKlass
    // 对照: parser.create_instance_klass()
    InstanceKlass* klass = parser.create_instance_klass();

    // 释放文件缓冲区
    FREE_C_HEAP_ARRAY(u1, buffer);

    return klass;
}

char* ClassLoader::class_name_to_file_path(const char* class_name) {
    // 将类名转换为文件路径
    // "HelloWorld"        → "<classpath>/HelloWorld.class"
    // "com/example/Main"  → "<classpath>/com/example/Main.class"
    // "com.example.Main"  → "<classpath>/com/example/Main.class"

    size_t cp_len = strlen(_classpath);
    size_t cn_len = strlen(class_name);
    size_t ext_len = 6;  // ".class"

    // +2: 一个 '/' 分隔符 + '\0'
    size_t path_len = cp_len + 1 + cn_len + ext_len + 1;
    char* path = (char*)malloc(path_len);
    if (path == nullptr) return nullptr;

    // 组装路径
    snprintf(path, path_len, "%s/", _classpath);
    size_t offset = cp_len + 1;

    // 复制类名，将 '.' 转换为 '/'
    for (size_t i = 0; i < cn_len; i++) {
        path[offset + i] = (class_name[i] == '.') ? '/' : class_name[i];
    }
    path[offset + cn_len] = '\0';

    // 追加 .class 扩展名
    strcat(path, ".class");

    return path;
}

u1* ClassLoader::read_file(const char* path, int* out_length) {
    FILE* f = fopen(path, "rb");
    if (f == nullptr) {
        return nullptr;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    u1* buffer = NEW_C_HEAP_ARRAY(u1, size, mtClass);
    if (buffer == nullptr) {
        fclose(f);
        return nullptr;
    }

    size_t read = fread(buffer, 1, size, f);
    fclose(f);

    if ((long)read != size) {
        FREE_C_HEAP_ARRAY(u1, buffer);
        return nullptr;
    }

    *out_length = (int)size;
    return buffer;
}
