#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "testing/unit_test.hpp"

import driver.types;

#include "driver/try.hpp"

using driver::Result;
using driver::Status;

namespace {

Status okStep() {
  return Status::Ok;
}
Status failStep() {
  return Status::Timeout;
}

Status chainOk() {
  DRV_TRY(okStep());
  DRV_TRY(okStep());
  return Status::Ok;
}

Status chainStopsOnError() {
  DRV_TRY(okStep());
  DRV_TRY(failStep());
  return Status::BusError;  // unreachable: DRV_TRY returns Timeout first
}

Result<int> readValue(bool ok) {
  return ok ? Result<int>{42} : Result<int>{Status::Nack};
}

Status useAssign(int &out, bool ok) {
  int value = 0;
  DRV_TRY_ASSIGN(value, readValue(ok));
  out = value;
  return Status::Ok;
}

void put(const char *s) {
  std::fputs(s, stdout);
}

}  // namespace

TEST(result_ok_holds_value) {
  Result<int> r{7};
  ASSERT_TRUE(r.ok());
  ASSERT_TRUE(static_cast<bool>(r));
  EXPECT_EQ(r.value(), 7);
  EXPECT_EQ(r.status(), Status::Ok);
}

TEST(result_error_path) {
  Result<int> r{Status::Timeout};
  ASSERT_FALSE(r.ok());
  EXPECT_EQ(r.status(), Status::Timeout);
  EXPECT_EQ(r.valueOr(-1), -1);
}

TEST(result_ok_status_normalises_to_none) {
  Result<int> r{Status::Ok};
  EXPECT_EQ(r.status(), Status::None);
}

TEST(drv_try_propagates_first_error) {
  EXPECT_EQ(chainOk(), Status::Ok);
  EXPECT_EQ(chainStopsOnError(), Status::Timeout);
}

TEST(drv_try_assign_binds_or_returns) {
  int value = 0;
  EXPECT_EQ(useAssign(value, true), Status::Ok);
  EXPECT_EQ(value, 42);
  EXPECT_EQ(useAssign(value, false), Status::Nack);
}

int main() {
  testing::TestRunner runner{put};
  RUN_TEST(runner, result_ok_holds_value);
  RUN_TEST(runner, result_error_path);
  RUN_TEST(runner, result_ok_status_normalises_to_none);
  RUN_TEST(runner, drv_try_propagates_first_error);
  RUN_TEST(runner, drv_try_assign_binds_or_returns);
  return runner.summary() ? 0 : 1;
}
