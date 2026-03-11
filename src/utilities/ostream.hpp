/*
 * my_jvm - Output Stream
 * 
 * 参考 OpenJDK 11 hotspot/src/hotspot/share/utilities/ostream.hpp
 * 简化版本：去除 timer 依赖
 */

#ifndef MY_JVM_UTILITIES_OSTREAM_HPP
#define MY_JVM_UTILITIES_OSTREAM_HPP

#include "memory/allocation.hpp"
#include "utilities/globalDefinitions.hpp"
#include <cstdio>
#include <cstdarg>
#include <cstring>

// ========== outputStream ==========

class outputStream : public ResourceObj {
 protected:
  int  _indentation;  // 当前缩进
  int  _width;        // 页面宽度
  int  _position;     // 当前行位置

  void update_position(const char* s, size_t len);

 public:
  outputStream(int width = 80) : _indentation(0), _width(width), _position(0) {}

  // 缩进
  outputStream& indent();
  void inc() { _indentation++; }
  void dec() { _indentation--; }
  void inc(int n) { _indentation += n; }
  void dec(int n) { _indentation -= n; }
  int indentation() const { return _indentation; }
  void set_indentation(int i) { _indentation = i; }
  void fill_to(int col);

  // 位置
  int width() const { return _width; }
  int position() const { return _position; }

  // 打印
  void print(const char* format, ...) ATTRIBUTE_PRINTF(2, 3);
  void print_cr(const char* format, ...) ATTRIBUTE_PRINTF(2, 3);
  void vprint(const char* format, va_list argptr) ATTRIBUTE_PRINTF(2, 0);
  void vprint_cr(const char* format, va_list argptr) ATTRIBUTE_PRINTF(2, 0);
  
  void print_raw(const char* str) { write(str, strlen(str)); }
  void print_raw(const char* str, int len) { write(str, len); }
  void print_raw_cr(const char* str) { write(str, strlen(str)); cr(); }
  void print_raw_cr(const char* str, int len) { write(str, len); cr(); }
  
  void put(char ch);
  void sp(int count = 1);
  void cr();
  void bol() { if (_position > 0) cr(); }

  // 64位整数打印
  void print_jlong(jlong value);
  void print_julong(julong value);

  // 刷新
  virtual void flush() {}
  virtual void write(const char* str, size_t len) = 0;
  virtual ~outputStream() {}

  void dec_cr() { dec(); cr(); }
  void inc_cr() { inc(); cr(); }
};

// ========== fileStream ==========

class fileStream : public outputStream {
 protected:
  FILE* _file;
  bool  _need_close;

 public:
  fileStream() : _file(nullptr), _need_close(false) {}
  fileStream(const char* file_name);
  fileStream(FILE* file) : _file(file), _need_close(false) {}
  ~fileStream();

  bool is_open() const { return _file != nullptr; }
  
  void write(const char* str, size_t len) override;
  void flush() override;
};

// ========== fdStream ==========

class fdStream : public outputStream {
 protected:
  int _fd;

 public:
  fdStream() : _fd(-1) {}
  fdStream(int fd) : _fd(fd) {}

  void set_fd(int fd) { _fd = fd; }
  int fd() const { return _fd; }
  
  void write(const char* str, size_t len) override;
};

// ========== stringStream ==========

class stringStream : public outputStream {
 protected:
  char*  _buffer;
  size_t _buffer_size;
  size_t _buffer_pos;
  bool   _owns_buffer;

 public:
  stringStream();
  stringStream(char* buffer, size_t buffer_size);
  ~stringStream();

  void write(const char* str, size_t len) override;
  
  const char* base() const { return _buffer; }
  char* as_string() const;
  size_t size() const { return _buffer_pos; }
  void reset() { _buffer_pos = 0; }
};

// ========== 标准输出 ==========

extern outputStream* tty;

// ========== outputStream 实现 ==========

inline void outputStream::update_position(const char* s, size_t len) {
  for (size_t i = 0; i < len; i++) {
    char ch = s[i];
    if (ch == '\n') {
      _position = 0;
    } else if (ch == '\t') {
      int tab_stop = 8;
      _position = ((_position / tab_stop) + 1) * tab_stop;
    } else {
      _position++;
    }
  }
}

inline outputStream& outputStream::indent() {
  for (int i = 0; i < _indentation; i++) {
    sp();
  }
  return *this;
}

inline void outputStream::fill_to(int col) {
  int need = col - _position;
  for (int i = 0; i < need; i++) {
    sp();
  }
}

inline void outputStream::put(char ch) {
  write(&ch, 1);
}

inline void outputStream::sp(int count) {
  for (int i = 0; i < count; i++) {
    write(" ", 1);
  }
}

inline void outputStream::cr() {
  write("\n", 1);
  _position = 0;
}

inline void outputStream::print_jlong(jlong value) {
  print(INT64_FORMAT, (int64_t)value);
}

inline void outputStream::print_julong(julong value) {
  print(UINT64_FORMAT, (uint64_t)value);
}

#endif // MY_JVM_UTILITIES_OSTREAM_HPP
