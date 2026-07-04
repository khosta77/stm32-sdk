module;
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
export module driver.circular_buffer;

import driver.types;

export namespace driver {

template <typename T, size_t N>
class CircularBuffer {
  static_assert((N & (N - 1)) == 0, "N must be power of 2");
  static constexpr size_t MASK = N - 1;

  std::array<T, N> _buf{};
  std::atomic<size_t> _head{0};
  std::atomic<size_t> _tail{0};

public:
  Status push(const T &item) {
    auto h = _head.load(std::memory_order_relaxed);
    auto t = _tail.load(std::memory_order_acquire);
    if (((h + 1) & MASK) == t) {
      return Status::Busy;
    }
    _buf[h] = item;
    _head.store((h + 1) & MASK, std::memory_order_release);
    return Status::Ok;
  }

  Status pop(T &item) {
    auto t = _tail.load(std::memory_order_relaxed);
    auto h = _head.load(std::memory_order_acquire);
    if (t == h) {
      return Status::Busy;
    }
    item = _buf[t];
    _tail.store((t + 1) & MASK, std::memory_order_release);
    return Status::Ok;
  }

  size_t read(T *dst, size_t maxLen) {
    size_t count = 0;
    while (count < maxLen) {
      T item;
      if (pop(item) != Status::Ok) {
        break;
      }
      dst[count++] = item;
    }
    return count;
  }

  size_t write(const T *src, size_t len) {
    size_t count = 0;
    while (count < len) {
      if (push(src[count]) != Status::Ok) {
        break;
      }
      ++count;
    }
    return count;
  }

  size_t size() const {
    auto h = _head.load(std::memory_order_acquire);
    auto t = _tail.load(std::memory_order_acquire);
    return (h - t) & MASK;
  }

  size_t free_space() const { return N - 1 - size(); }
  bool empty() const {
    return _head.load(std::memory_order_acquire) ==
           _tail.load(std::memory_order_acquire);
  }
  bool full() const { return free_space() == 0; }
  constexpr size_t capacity() const { return N - 1; }
};

}  // namespace driver
