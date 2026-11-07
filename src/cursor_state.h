/**
 * @file src/cursor_state.h
 * @brief Tells opted-in clients whether a game has captured the host cursor (Apollo extension).
 *
 * A game that uses the mouse for its camera hides the cursor and/or clips it to its window. A
 * client that streams in a window wants a captured, relative mouse then, and a free, absolute
 * mouse on the desktop. Apollo watches the cursor and sends the settled state to clients that
 * asked for it.
 *
 * Negotiation (stock clients and stock hosts never see anything new):
 * - The host advertises SDP_ATTRIBUTE in its RTSP DESCRIBE response.
 * - A client that wants the messages adds `a=<ENABLE_ARG>:1` to its ANNOUNCE.
 * - The host then sends one PACKET_TYPE control message with the current state as soon as the
 *   control stream connects, and another each time the state changes.
 *
 * Message payload (after the usual 4-byte control header, encrypted like every control message):
 *   uint8 version (1), uint8 flags (bit 0: captured)
 * Clients ignore versions they don't know.
 *
 * The client side lives next to CURSOR_STATE_SDP_ATTRIBUTE in danielzn-moonlight-qt's
 * moonlight-common-c (Limelight-internal.h).
 */
#pragma once

// standard includes
#include <array>
#include <chrono>
#include <cstdint>
#include <string_view>

namespace stream::cursor {
  constexpr std::string_view SDP_ATTRIBUTE = "a=x-apollo-cursor:1";
  constexpr std::string_view ENABLE_ARG = "x-apollo-cursor.enable";
  constexpr std::uint16_t PACKET_TYPE = 0x3080;
  constexpr std::uint8_t VERSION = 1;
  constexpr std::uint8_t FLAG_CAPTURED = 0x01;

  /**
   * @brief One look at the host cursor.
   */
  struct sample_t {
    bool valid = false;  ///< False when the cursor couldn't be read (e.g. the secure desktop): counts as free.
    bool visible = true;  ///< The cursor is showing (GetCursorInfo CURSOR_SHOWING).
    bool clipped = false;  ///< The cursor is confined to less than the whole desktop (ClipCursor).
    bool hides_while_typing = false;  ///< Windows' "Hide pointer while typing" is on.
    bool last_input_keyboard = false;  ///< The last input Apollo injected was a key, not the mouse.
    std::uint64_t mouse_events = 0;  ///< Count of mouse events Apollo has injected.
  };

  /**
   * @brief Turns cursor samples into a settled captured/free state.
   *
   * Captured means hidden or clipped. Two things are filtered out:
   * - "Hide pointer while typing": the cursor hides right after a key with no mouse input in
   *   between. That stays free until the mouse moves (then it either shows again, or a game is
   *   really hiding it).
   * - Flicker: a new state only counts once it has held for `settle`.
   */
  class detector_t {
  public:
    using clock = std::chrono::steady_clock;

    explicit detector_t(std::chrono::milliseconds settle = std::chrono::milliseconds {250}):
        _settle {settle} {}

    /**
     * @brief Feed one sample.
     * @return The settled state: true while the cursor is captured.
     */
    bool update(const sample_t &sample, clock::time_point now);

    /**
     * @brief The settled state (false before the first sample).
     */
    bool captured() const {
      return _captured;
    }

    /**
     * @brief Forget everything (no client wants the state any more).
     */
    void reset() {
      *this = detector_t {_settle};
    }

  private:
    bool raw(const sample_t &sample);

    std::chrono::milliseconds _settle;
    bool _started = false;
    bool _captured = false;
    bool _pending = false;
    clock::time_point _pending_since;
    sample_t _prev;
    bool _vanished = false;
  };

  /**
   * @brief The control message (4-byte header + payload), before encryption.
   */
  std::array<std::uint8_t, 6> encode_message(bool captured);
}  // namespace stream::cursor
