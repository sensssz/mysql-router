#ifndef UTILS_SPSC_RING_BUFFER_H_
#define UTILS_SPSC_RING_BUFFER_H_

#include <atomic>
#include <memory>

const uint32_t kDefaultBufferSize = 1e+7;

class SpscRingBuffer {
public:
  SpscRingBuffer() : SpscRingBuffer(kDefaultBufferSize) {}
  SpscRingBuffer(size_t buf_size);
  void SignalError() {
    error_ = true;
  }
  void ClearError() {
    error_ = false;
  }
  bool HasError() {
    return error_;
  }
  bool HasData() {
    return read_loc_.load() != write_loc_.load();
  }
  ssize_t Read(void *buffer, ssize_t size);
  void Write(const char *data, size_t size);

private:
  size_t DataSize();
  char *BufGet(size_t loc) {
    return buffer_.get() + loc;
  }

  bool error_;
  std::atomic<size_t> read_loc_;
  std::atomic<size_t> write_loc_;
  size_t buf_size_;
  std::unique_ptr<char[]> buffer_;
};

#endif // UTILS_SPSC_RING_BUFFER_H_
