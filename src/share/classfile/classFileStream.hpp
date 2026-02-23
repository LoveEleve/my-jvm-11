#ifndef SHARE_CLASSFILE_CLASSFILESTREAM_HPP
#define SHARE_CLASSFILE_CLASSFILESTREAM_HPP

// ============================================================================
// Mini JVM - Class 文件字节流
// 对应 HotSpot: src/hotspot/share/classfile/classFileStream.hpp
//
// 职责：封装 .class 文件的字节缓冲区，提供顺序读取 u1/u2/u4/u8 的能力。
//
// 设计要点（和 HotSpot 一致）：
//   1. 构造时接收已加载到内存的完整字节缓冲区
//   2. 维护一个"当前位置"游标 (_current)，每次读取后自动前移
//   3. 提供边界检查，防止读取越界
//   4. 使用 Bytes::get_Java_uN() 处理字节序转换
//
// 和 HotSpot 的差异：
//   - HotSpot 的 ClassFileStream 继承 ResourceObj（线程本地资源区分配）
//   - 我们暂时不继承任何基类，后续添加 ResourceObj 后再改
//   - HotSpot 有 TRAPS 异常传播机制，我们用返回值 + 错误标志代替
// ============================================================================

#include "utilities/globalDefinitions.hpp"
#include "utilities/debug.hpp"
#include "utilities/bytes.hpp"

class ClassFileStream {
private:
    const u1* const _buffer_start;   // 缓冲区起始（不可变）
    const u1* const _buffer_end;     // 缓冲区终止（past-the-end，不可变）
    mutable const u1* _current;      // 当前读取位置（mutable: 允许在 const 方法中前移）
    const char* const _source;       // 来源描述（文件路径等）

public:
    // 构造函数
    ClassFileStream(const u1* buffer, int length, const char* source)
        : _buffer_start(buffer),
          _buffer_end(buffer + length),
          _current(buffer),
          _source(source) {
        guarantee(buffer != nullptr || length == 0, "buffer must not be null");
    }

    // ========== 位置查询 ==========

    const u1* buffer()  const { return _buffer_start; }
    int length()        const { return (int)(_buffer_end - _buffer_start); }
    const u1* current() const { return _current; }
    const char* source() const { return _source; }

    // 已读取的偏移量
    int current_offset() const {
        return (int)(_current - _buffer_start);
    }

    // 是否已读到末尾
    bool at_eos() const { return _current >= _buffer_end; }

    // 剩余字节数
    int remaining() const { return (int)(_buffer_end - _current); }

    // ========== 边界检查 ==========

    // 检查是否还有至少 size 字节可读
    bool check_remaining(int size) const {
        return (_buffer_end - _current) >= size;
    }

    // 带错误报告的边界检查
    void guarantee_more(int size) const {
        if (!check_remaining(size)) {
            truncated_file_error();
        }
    }

    void truncated_file_error() const {
        fprintf(stderr, "ClassFormatError: Truncated class file [source: %s, "
                "offset: %d, remaining: %d]\n",
                _source ? _source : "<unknown>",
                current_offset(), remaining());
        fatal("Truncated class file");
    }

    // ========== 读取方法（带边界检查）==========
    // 对应 HotSpot 的 get_uN(TRAPS) 版本

    u1 get_u1() const {
        guarantee_more(1);
        return *_current++;
    }

    u2 get_u2() const {
        guarantee_more(2);
        const u1* tmp = _current;
        _current += 2;
        return Bytes::get_Java_u2((address)tmp);
    }

    u4 get_u4() const {
        guarantee_more(4);
        const u1* tmp = _current;
        _current += 4;
        return Bytes::get_Java_u4((address)tmp);
    }

    u8 get_u8() const {
        guarantee_more(8);
        const u1* tmp = _current;
        _current += 8;
        return Bytes::get_Java_u8((address)tmp);
    }

    // ========== 快速读取（无边界检查）==========
    // 对应 HotSpot 的 get_uN_fast() 版本
    // 调用者已确保有足够字节

    u1 get_u1_fast() const {
        return *_current++;
    }

    u2 get_u2_fast() const {
        u2 res = Bytes::get_Java_u2((address)_current);
        _current += 2;
        return res;
    }

    u4 get_u4_fast() const {
        u4 res = Bytes::get_Java_u4((address)_current);
        _current += 4;
        return res;
    }

    u8 get_u8_fast() const {
        u8 res = Bytes::get_Java_u8((address)_current);
        _current += 8;
        return res;
    }

    // ========== 跳过方法 ==========

    void skip_u1(int length) const {
        guarantee_more(length);
        _current += length;
    }

    void skip_u2(int length) const {
        guarantee_more(length * 2);
        _current += length * 2;
    }

    void skip_u4(int length) const {
        guarantee_more(length * 4);
        _current += length * 4;
    }

    void skip_u1_fast(int length) const { _current += length; }
    void skip_u2_fast(int length) const { _current += length * 2; }
    void skip_u4_fast(int length) const { _current += length * 4; }

    // ========== 获取当前位置的原始指针 ==========
    // 用于批量读取（如 UTF-8 字符串内容）

    const u1* get_u1_buffer() const { return _current; }

    NONCOPYABLE(ClassFileStream);
};

#endif // SHARE_CLASSFILE_CLASSFILESTREAM_HPP
