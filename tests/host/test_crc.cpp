#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "testing/unit_test.hpp"

import driver.crc;
import driver.soft_crc;

using driver::Crc32Spec;
using driver::SoftCrc;
using driver::softCrcStep;

namespace {

// The algorithm's standard check string.
const uint8_t kCheck[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};

// Reference data plus the expected CRC-32 of each of its prefixes. Generated
// with:
//   python3 -c 'import zlib; s=b"abcdefghijkl"
//   print([hex(zlib.crc32(s[:i])) for i in range(13)])'
// Lengths 0..12 cover every remainder modulo 4, which is what the F4 hardware
// driver has to get right when it splits a buffer into words plus a tail.
const uint8_t kAlphabet[] =
    {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l'};

const uint32_t kAlphabetCrc[] = {
    0x00000000,
    0xE8B7BE43,
    0x9E83486D,
    0x352441C2,
    0xED82CD11,
    0x8587D865,
    0x4B8E39EF,
    0x312A6AA6,
    0xAEEF2A50,
    0x8DA988AF,
    0x3981703A,
    0xCE570F9F,
    0xF6781B24,
};

void put(const char *s) {
  std::fputs(s, stdout);
}

}  // namespace

TEST(soft_crc_matches_the_standard_check_value) {
  SoftCrc crc;
  EXPECT_EQ(crc.compute({kCheck, 9}), Crc32Spec::CHECK);
  EXPECT_EQ(Crc32Spec::CHECK, 0xCBF43926U);
}

TEST(soft_crc_of_empty_input_is_zero) {
  SoftCrc crc;
  EXPECT_EQ(crc.compute({}), 0x00000000U);
}

TEST(soft_crc_matches_zlib_for_every_length) {
  for (size_t len = 0; len <= 12; ++len) {
    SoftCrc crc;
    EXPECT_EQ(crc.compute({kAlphabet, len}), kAlphabetCrc[len]);
  }
}

TEST(soft_crc_streaming_equals_one_shot) {
  // Every split of "123456789" into two chunks must agree with one call --
  // including the splits that leave a partial word on either side.
  for (size_t cut = 0; cut <= 9; ++cut) {
    SoftCrc streamed;
    streamed.reset();
    streamed.update({kCheck, cut});
    streamed.update({kCheck + cut, 9 - cut});
    EXPECT_EQ(streamed.value(), Crc32Spec::CHECK);
  }
}

TEST(soft_crc_streams_in_tiny_chunks) {
  SoftCrc streamed;
  streamed.reset();
  for (size_t i = 0; i < 12; ++i) {
    streamed.update({kAlphabet + i, 1});
  }
  EXPECT_EQ(streamed.value(), kAlphabetCrc[12]);
}

TEST(soft_crc_reset_restores_the_initial_state) {
  SoftCrc crc;
  (void) crc.compute({kAlphabet, 12});
  crc.reset();
  EXPECT_EQ(crc.value(), 0x00000000U);
  crc.update({kCheck, 9});
  EXPECT_EQ(crc.value(), Crc32Spec::CHECK);
}

TEST(soft_crc_value_is_a_pure_read) {
  SoftCrc crc;
  const uint32_t first = crc.compute({kCheck, 9});
  EXPECT_EQ(crc.value(), first);
  EXPECT_EQ(crc.value(), first);
}

TEST(soft_crc_step_is_the_reflected_polynomial) {
  // The step the F4 driver borrows for its tail bytes: feeding the whole
  // string through it by hand must reproduce compute().
  uint32_t state = Crc32Spec::INIT;
  for (size_t i = 0; i < 9; ++i) {
    state = softCrcStep(state, kCheck[i]);
  }
  EXPECT_EQ(state ^ Crc32Spec::XOR_OUT, Crc32Spec::CHECK);
}

int main() {
  testing::TestRunner runner{put};
  RUN_TEST(runner, soft_crc_matches_the_standard_check_value);
  RUN_TEST(runner, soft_crc_of_empty_input_is_zero);
  RUN_TEST(runner, soft_crc_matches_zlib_for_every_length);
  RUN_TEST(runner, soft_crc_streaming_equals_one_shot);
  RUN_TEST(runner, soft_crc_streams_in_tiny_chunks);
  RUN_TEST(runner, soft_crc_reset_restores_the_initial_state);
  RUN_TEST(runner, soft_crc_value_is_a_pure_read);
  RUN_TEST(runner, soft_crc_step_is_the_reflected_polynomial);
  return runner.summary() ? 0 : 1;
}
