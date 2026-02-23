#ifndef SHARE_CLASSFILE_CLASSLOADER_HPP
#define SHARE_CLASSFILE_CLASSLOADER_HPP

// ============================================================================
// Mini JVM - ClassLoader（Bootstrap 类加载器）
// 对照 HotSpot: src/hotspot/share/classfile/classLoader.hpp
//
// HotSpot 的 ClassLoader 负责:
//   1. 维护 boot classpath 条目列表
//   2. 从 classpath 搜索 .class 文件
//   3. 支持 jimage（模块镜像）、目录、jar 三种格式
//
// 简化版：只支持目录形式的 classpath
// 从目录中搜索 .class 文件，读取为字节流，然后调用 ClassFileParser 解析
//
// 加载链:
//   ClassLoader::load_class("HelloWorld")
//     → 搜索 <classpath>/HelloWorld.class
//     → 读取文件为 ClassFileStream
//     → ClassFileParser 解析
//     → 创建 InstanceKlass
// ============================================================================

#include "utilities/globalDefinitions.hpp"

class InstanceKlass;
class ClassFileStream;

class ClassLoader {
public:
    // 初始化 — 设置 classpath
    // 对照: classLoader_init1() [init.cpp:113]
    static void initialize();

    // 从 classpath 加载类
    // 对照: ClassLoader::load_class() [classLoader.cpp:1434]
    //
    // 输入: class_name — 类名（如 "HelloWorld" 或 "com/example/Main"）
    //       用 '/' 分隔包名（与 JVM 内部格式一致）
    //       也接受 '.' 分隔（会自动转换）
    //
    // 输出: InstanceKlass* 已解析但未链接；失败返回 nullptr
    static InstanceKlass* load_class(const char* class_name);

private:
    // classpath（从 Arguments 获取）
    static const char* _classpath;

    // 将类名转换为文件路径
    // "HelloWorld" → "<classpath>/HelloWorld.class"
    // "com/example/Main" → "<classpath>/com/example/Main.class"
    // "com.example.Main" → "<classpath>/com/example/Main.class"（自动转换点号）
    static char* class_name_to_file_path(const char* class_name);

    // 读取文件内容
    // 返回 malloc 分配的缓冲区，out_length 输出文件大小
    static u1* read_file(const char* path, int* out_length);
};

#endif // SHARE_CLASSFILE_CLASSLOADER_HPP
