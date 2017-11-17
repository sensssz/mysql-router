#include "mysqlrouter/spsc_ring_buffer.h"

#include <cstring>

template<typename T>
static size_t ReadValue(char *buffer, size_t loc, T &value) {
  value = *(reinterpret_cast<T *>(buffer + loc));
  return loc + sizeof(T);
}

template<typename T>
static size_t WriteValue(char *buffer, size_t loc, const T &value) {
  *(reinterpret_cast<T *>(buffer + loc)) = value;
  return loc + sizeof(T);
}

template<typename T>
static T min(T num1, T num2) {
  return num1 < num2 ? num1 : num2;
}

SpscRingBuffer::SpscRingBuffer(size_t buf_size) :
    read_loc_(0), write_loc_(0), buf_size_(buf_size), buffer_(new char[buf_size]) {}

ssize_t SpscRingBuffer::Read(void *buffer, ssize_t size) {
  ssize_t read_loc = 0;
  ssize_t write_loc = 0;
  ssize_t size_left = 0;
  ssize_t read_size = 0;
  while (!HasData()) {
    // Left empty.
  }
  read_loc = read_loc_.load();
  write_loc = write_loc_.load();
  if (read_loc < write_loc) {
    size_left = write_loc - read_loc;
    read_size = min(size_left, size);
    memcpy(buffer, buffer_.get() + read_loc, read_size);
    read_loc_ = read_loc + read_size;
  } else {
    size_left = static_cast<ssize_t>(buf_size_) - read_loc;
    read_size = min(size_left, size);
    memcpy(buffer, buffer_.get() + read_loc, read_size);
    size -= read_size;
    if (size > 0) {
      // Wrap to the start and read the rest
      size = min(size, write_loc);
      memcpy(buffer + read_size, buffer_.get(), size);
      read_size += size;
      read_loc_ = size;
    } else {
      read_loc_ = read_loc + read_size;
    }
  }
  return (size_t) read_size;
}

void SpscRingBuffer::Write(const char *data, size_t size) {
  ssize_t write_loc = write_loc_.load();
  ssize_t size_left = buf_size_ - write_loc;
  if (size > buf_size_) {
    return;
  }
  while (DataSize() + size > buf_size_) {
    // Left empty.
  }
  if ((ssize_t) size > size_left) {
    memcpy(buffer_.get() + write_loc, data, size_left);
    memcpy(buffer_.get(), data + size_left, size - size_left);
    write_loc_ = static_cast<ssize_t>(size - size_left);
  } else {
    memcpy(buffer_.get() + write_loc, data, size);
    write_loc_ = static_cast<ssize_t>(write_loc + size);
  }
}

size_t SpscRingBuffer::DataSize() {
  size_t read_loc = read_loc_.load();
  size_t write_loc = write_loc_.load();
  if (write_loc < read_loc) {
    return (buf_size_ - read_loc) + write_loc;
  }
  return write_loc - read_loc;
}
