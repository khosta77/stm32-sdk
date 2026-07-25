#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "testing/unit_test.hpp"

import driver.types;
import driver.circular_buffer;

using driver::CircularBuffer;
using driver::Status;

namespace {
void put(const char *s) {
  std::fputs(s, stdout);
}
}  // namespace

TEST(cbuf_push_pop_fifo) {
  CircularBuffer<uint8_t, 8> cb;  // usable capacity N-1 = 7
  EXPECT_TRUE(cb.empty());
  EXPECT_EQ(cb.free_space(), static_cast<size_t>(7));
  EXPECT_EQ(cb.capacity(), static_cast<size_t>(7));

  EXPECT_EQ(cb.push(1), Status::Ok);
  EXPECT_EQ(cb.push(2), Status::Ok);
  EXPECT_EQ(cb.size(), static_cast<size_t>(2));
  EXPECT_FALSE(cb.empty());

  uint8_t v = 0;
  EXPECT_EQ(cb.pop(v), Status::Ok);
  EXPECT_EQ(v, 1);
  EXPECT_EQ(cb.pop(v), Status::Ok);
  EXPECT_EQ(v, 2);
  EXPECT_TRUE(cb.empty());
  EXPECT_EQ(cb.pop(v), Status::Busy);  // pop on empty is rejected
}

TEST(cbuf_full_rejects_push) {
  CircularBuffer<uint8_t, 4> cb;  // usable capacity 3
  EXPECT_EQ(cb.push(10), Status::Ok);
  EXPECT_EQ(cb.push(20), Status::Ok);
  EXPECT_EQ(cb.push(30), Status::Ok);
  EXPECT_TRUE(cb.full());
  EXPECT_EQ(cb.free_space(), static_cast<size_t>(0));
  EXPECT_EQ(cb.push(40), Status::Busy);  // push on full is rejected
}

TEST(cbuf_wraparound_preserves_order) {
  CircularBuffer<uint8_t, 4> cb;  // usable capacity 3
  uint8_t v = 0;
  for (uint8_t i = 0; i < 20; ++i) {
    EXPECT_EQ(cb.push(i), Status::Ok);
    EXPECT_EQ(cb.pop(v), Status::Ok);
    EXPECT_EQ(v, i);
  }
  EXPECT_TRUE(cb.empty());
}

TEST(cbuf_bulk_read_write) {
  CircularBuffer<uint8_t, 8> cb;  // usable capacity 7
  const uint8_t src[5] = {1, 2, 3, 4, 5};
  EXPECT_EQ(cb.write(src, 5), static_cast<size_t>(5));
  EXPECT_EQ(cb.size(), static_cast<size_t>(5));

  uint8_t dst[5] = {0, 0, 0, 0, 0};
  EXPECT_EQ(cb.read(dst, 5), static_cast<size_t>(5));
  EXPECT_EQ(dst[0], 1);
  EXPECT_EQ(dst[4], 5);
  EXPECT_TRUE(cb.empty());
}

TEST(cbuf_bulk_write_stops_at_capacity) {
  CircularBuffer<uint8_t, 8> cb;  // usable capacity 7
  const uint8_t big[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_EQ(cb.write(big, 10), static_cast<size_t>(7));  // only 7 fit
  EXPECT_TRUE(cb.full());
}

int main() {
  testing::TestRunner runner{put};
  RUN_TEST(runner, cbuf_push_pop_fifo);
  RUN_TEST(runner, cbuf_full_rejects_push);
  RUN_TEST(runner, cbuf_wraparound_preserves_order);
  RUN_TEST(runner, cbuf_bulk_read_write);
  RUN_TEST(runner, cbuf_bulk_write_stops_at_capacity);
  return runner.summary() ? 0 : 1;
}
