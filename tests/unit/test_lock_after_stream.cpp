/**
 * @file tests/unit/test_lock_after_stream.cpp
 * @brief Test src/lock_after_stream.h: when the PC locks after a stream ends.
 */
#include "../tests_common.h"

#include <src/lock_after_stream.h>

using namespace std::literals;
using steady = lock_after_stream::clock;

namespace {
  const steady::time_point t0 {};

  // A stream from t0 until `end`, then checks every second up to `until`; the time it locked (or none).
  std::optional<steady::duration> lock_time(int delay, std::chrono::seconds end, std::chrono::seconds until, steady::time_point last_input = t0) {
    lock_after_stream::timer_t timer;
    for (auto t = 0s; t <= until; t += 1s) {
      if (timer.update(delay, t < end, t0 + t, last_input)) {
        return t;
      }
    }
    return std::nullopt;
  }
}  // namespace

TEST(LockAfterStreamTest, LocksTheDelayAfterTheLastClientLeaves) {
  EXPECT_EQ(lock_time(5, 60s, 20min), 60s + 5min);
  EXPECT_EQ(lock_time(1, 10s, 20min), 10s + 1min);
}

TEST(LockAfterStreamTest, ImmediatelyLocksAtTheFirstCheckAfterTheStream) {
  EXPECT_EQ(lock_time(0, 10s, 1min), 10s);
}

TEST(LockAfterStreamTest, OffNeverLocks) {
  EXPECT_EQ(lock_time(-1, 10s, 2h), std::nullopt);
}

TEST(LockAfterStreamTest, NoStreamNoLock) {
  // Apollo started with nobody streaming: the PC is left alone
  EXPECT_EQ(lock_time(0, 0s, 2h), std::nullopt);
}

TEST(LockAfterStreamTest, LocksOncePerStream) {
  lock_after_stream::timer_t timer;
  int locks = 0;
  for (auto t = 0s; t <= 2h; t += 1s) {
    locks += timer.update(1, t < 10s, t0 + t, t0);
  }
  EXPECT_EQ(locks, 1);
}

TEST(LockAfterStreamTest, AReconnectWithinTheDelayCancelsIt) {
  lock_after_stream::timer_t timer;
  // Streams until 10 s, back at 3 min (inside the 5 minutes) until 20 min
  for (auto t = 0s; t <= 20min + 4min; t += 1s) {
    const bool streaming = t < 10s || (t >= 3min && t < 20min);
    ASSERT_FALSE(timer.update(5, streaming, t0 + t, t0)) << "locked at " << t.count() << " s";
  }
  // The timer runs from the second stream's end
  EXPECT_TRUE(timer.update(5, false, t0 + 25min, t0));
}

TEST(LockAfterStreamTest, LocalInputPushesTheLockBack) {
  lock_after_stream::timer_t timer;
  // Stream ends at 10 s; someone at the PC types at 3 min and at 6 min
  std::optional<std::chrono::seconds> locked;
  for (auto t = 0s; t <= 30min && !locked; t += 1s) {
    const auto input = t0 + (t >= 6min ? 6min : t >= 3min ? 3min : 0s);
    if (timer.update(5, t < 10s, t0 + t, input)) {
      locked = t;
    }
  }
  // Not under them: 5 minutes after their last input
  EXPECT_EQ(locked, 11min);
}

TEST(LockAfterStreamTest, InputDuringTheStreamDoesNotShortenTheDelay) {
  // The client's own input stops when it leaves: the delay still counts from the end
  EXPECT_EQ(lock_time(5, 10min, 30min, t0 + 9min), 15min);
}

TEST(LockAfterStreamTest, TurningItOffCancelsAPendingLock) {
  lock_after_stream::timer_t timer;
  EXPECT_FALSE(timer.update(5, true, t0, t0));
  EXPECT_FALSE(timer.update(5, false, t0 + 1s, t0));
  EXPECT_FALSE(timer.update(-1, false, t0 + 2min, t0));
  EXPECT_FALSE(timer.update(5, false, t0 + 10min, t0));
}

TEST(LockAfterStreamTest, WinLFromAClientIsTheLockShortcut) {
  constexpr uint8_t win = 0x08, alt = 0x04, ctrl = 0x02;
  EXPECT_TRUE(lock_after_stream::is_lock_shortcut('L', false, win));
  EXPECT_TRUE(lock_after_stream::is_lock_shortcut('L', false, win | ctrl));
  // Only the press locks; its release, a plain L and other Win shortcuts pass through
  EXPECT_FALSE(lock_after_stream::is_lock_shortcut('L', true, win));
  EXPECT_FALSE(lock_after_stream::is_lock_shortcut('L', false, 0));
  EXPECT_FALSE(lock_after_stream::is_lock_shortcut('L', false, alt));
  EXPECT_FALSE(lock_after_stream::is_lock_shortcut('D', false, win));
}
