/*
 * my_jvm - Output Stream implementation
 */

#include "utilities/ostream.hpp"
#include <cstdlib>
#include <unistd.h>

// ========== 全局输出流 ==========

outputStream* tty = nullptr;

// ========== outputStream 实现 ==========

void outputStream::print(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vprint(format, args);
  va_end(args);
}

void outputStream::print_cr(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vprint_cr(format, args);
  va_end(args);
}

void outputStream::vprint(const char* format, va_list argptr) {
  char buffer[1024];
  int len = vsnprintf(buffer, sizeof(buffer), format, argptr);
  if (len > 0) {
    write(buffer, (size_t)len);
  }
}

void outputStream::vprint_cr(const char* format, va_list argptr) {
  vprint(format, argptr);
  cr();
}

// ========== fileStream 实现 ==========

fileStream::fileStream(const char* file_name) {
  _file = fopen(file_name, "w");
  _need_close = (_file != nullptr);
}

fileStream::~fileStream() {
  if (_need_close && _file != nullptr) {
    fclose(_file);
    _file = nullptr;
  }
}

void fileStream::write(const char* str, size_t len) {
  if (_file != nullptr) {
    fwrite(str, 1, len, _file);
    update_position(str, len);
  }
}

void fileStream::flush() {
  if (_file != nullptr) {
    fflush(_file);
  }
}

// ========== fdStream 实现 ==========

void fdStream::write(const char* str, size_t len) {
  if (_fd >= 0) {
    ::write(_fd, str, len);
    update_position(str, len);
  }
}

// ========== stringStream 实现 ==========

stringStream::stringStream() 
  : _buffer(nullptr), _buffer_size(0), _buffer_pos(0), _owns_buffer(true) {
  _buffer_size = 1024;
  _buffer = (char*)AllocateHeap(_buffer_size, mtInternal);
}

stringStream::stringStream(char* buffer, size_t buffer_size)
  : _buffer(buffer), _buffer_size(buffer_size), _buffer_pos(0), _owns_buffer(false) {
}

stringStream::~stringStream() {
  if (_owns_buffer && _buffer != nullptr) {
    FreeHeap(_buffer);
    _buffer = nullptr;
  }
}

void stringStream::write(const char* str, size_t len) {
  // 检查是否需要扩展
  if (_buffer_pos + len >= _buffer_size) {
    size_t new_size = _buffer_size * 2;
    while (new_size <= _buffer_pos + len) {
      new_size *= 2;
    }
    
    if (_owns_buffer) {
      char* new_buffer = (char*)AllocateHeap(new_size, mtInternal);
      memcpy(new_buffer, _buffer, _buffer_pos);
      FreeHeap(_buffer);
      _buffer = new_buffer;
      _buffer_size = new_size;
    } else {
      // 不能扩展，截断
      len = _buffer_size - _buffer_pos - 1;
      if (len <= 0) return;
    }
  }
  
  memcpy(_buffer + _buffer_pos, str, len);
  _buffer_pos += len;
  _buffer[_buffer_pos] = '\0';
  update_position(str, len);
}

char* stringStream::as_string() const {
  char* copy = (char*)AllocateHeap(_buffer_pos + 1, mtInternal);
  memcpy(copy, _buffer, _buffer_pos + 1);
  return copy;
}
