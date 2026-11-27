/**
 * @file src/lock_after_stream.h
 * @brief Locks the PC a set time after the last stream ends (config `lock_after_stream`).
 */
#pragma once

// standard includes
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>

namespace lock_after_stream {
  using clock = std::chrono::steady_clock;

  /**
   * @brief When to lock. The timer starts when the last client leaves; a stream starting again cancels it, and
   * local input pushes it back, so the PC locks only once nobody has touched it for the whole delay.
   */
  class timer_t {
  public:
    /**
     * @param delay_minutes The setting: -1 is off, 0 is immediately.
     * @param streaming Whether any client streams now.
     * @param last_input When the PC last had keyboard or mouse input.
     * @return true once, when it's time to lock.
     */
    bool update(int delay_minutes, bool streaming, clock::time_point now, clock::time_point last_input) {
      if (streaming || delay_minutes < 0) {
        streamed = streaming;
        ended.reset();
        return false;
      }
      if (streamed) {
        streamed = false;
        ended = now;
      }
      if (!ended || now - std::max(*ended, last_input) < std::chrono::minutes(delay_minutes)) {
        return false;
      }
      ended.reset();
      return true;
    }

  private:
    bool streamed = false;
    std::optional<clock::time_point> ended;
  };

  /**
   * @brief Whether a key press from a client is Win+L. Windows ignores Win+L when it comes from injected input,
   * so Apollo locks the PC itself instead of passing the L on.
   * @param key_code The Windows virtual key.
   * @param release Whether the key goes up.
   * @param modifiers The packet's modifier flags (Win is 0x08, MODIFIER_META).
   */
  inline bool is_lock_shortcut(uint16_t key_code, bool release, uint8_t modifiers) {
    return !release && key_code == 'L' && (modifiers & 0x08);
  }

  /**
   * @brief Locks the signed-in user's session; does nothing when it's already locked or nobody is signed in (Windows only).
   * @param reason Says in the log what asked for the lock.
   */
  void lock(const char *reason);

  /**
   * @brief Checks once a second until shutdown and locks the signed-in session when the timer says so (Windows only).
   */
  void run();
}  // namespace lock_after_stream
