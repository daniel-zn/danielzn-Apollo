/**
 * @file src/cursor_state.cpp
 * @brief Cursor capture detection and message encoding; see cursor_state.h.
 */
#include "cursor_state.h"

namespace stream::cursor {
  bool detector_t::raw(const sample_t &sample) {
    if (!sample.valid) {
      _vanished = false;
      _prev = sample;
      return false;
    }

    bool moused = _prev.valid && sample.mouse_events != _prev.mouse_events;
    if (_prev.valid && _prev.visible && !sample.visible && sample.hides_while_typing &&
        sample.last_input_keyboard && !moused) {
      // Windows hid the pointer because the user is typing
      _vanished = true;
    }
    if (sample.visible || moused) {
      _vanished = false;
    }

    _prev = sample;
    return sample.clipped || (!sample.visible && !_vanished);
  }

  bool detector_t::update(const sample_t &sample, clock::time_point now) {
    bool state = raw(sample);

    if (!_started) {
      _started = true;
      _captured = state;
      _pending = false;
      return _captured;
    }

    if (state == _captured) {
      _pending = false;
    } else if (!_pending) {
      _pending = true;
      _pending_since = now;
    }

    if (_pending && now - _pending_since >= _settle) {
      _captured = state;
      _pending = false;
    }

    return _captured;
  }

  std::array<std::uint8_t, 6> encode_message(bool captured) {
    return {
      static_cast<std::uint8_t>(PACKET_TYPE & 0xFF),
      static_cast<std::uint8_t>(PACKET_TYPE >> 8),
      2,  // payload length, little-endian
      0,
      VERSION,
      static_cast<std::uint8_t>(captured ? FLAG_CAPTURED : 0),
    };
  }
}  // namespace stream::cursor
