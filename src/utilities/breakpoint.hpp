/*
 * my_jvm - Breakpoint support
 * 
 * 参考 OpenJDK 11 hotspot/src/hotspot/share/utilities/breakpoint.hpp
 * 简化版本，用于调试支持
 */

#ifndef MY_JVM_UTILITIES_BREAKPOINT_HPP
#define MY_JVM_UTILITIES_BREAKPOINT_HPP

// 断点宏定义
// 如果没有平台特定定义，默认调用外部函数
#ifndef BREAKPOINT
extern "C" void breakpoint();
#define BREAKPOINT ::breakpoint()
#endif

#endif // MY_JVM_UTILITIES_BREAKPOINT_HPP
