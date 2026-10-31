/**
 * @file src/display_switch.h
 * @brief What to do with a running virtual display when a client launches or resumes (see docs/display_scale.md).
 */
#pragma once

// standard includes
#include <cstdint>
#include <span>

namespace display_switch {
  /**
   * @brief A display mode as Windows reports it (whole hertz).
   */
  struct display_mode_t {
    uint32_t width;
    uint32_t height;
    int hz;
  };

  /**
   * @brief Whether a display mode is width x height at fps (mHz), give or take 1 Hz of rounding.
   */
  constexpr bool matches(const display_mode_t &mode, uint32_t width, uint32_t height, int fps) {
    const int off = mode.hz * 1000 - fps;
    return mode.width == width && mode.height == height && off >= -1000 && off <= 1000;
  }

  /**
   * @brief Whether a mode change held: every reading taken after it (at least one) shows the requested mode.
   * Windows can report a new mode right after a change and fall back to the old one moments later.
   */
  constexpr bool held(std::span<const display_mode_t> readings, uint32_t width, uint32_t height, int fps) {
    if (readings.empty()) {
      return false;
    }
    for (const auto &mode : readings) {
      if (!matches(mode, width, height, fps)) {
        return false;
      }
    }
    return true;
  }

  enum class step_e {
    keep,  ///< The display already has the client's mode.
    change_in_place,  ///< Set the client's mode on the display as it is.
    recreate,  ///< Remove the display and create it again at the client's mode.
  };

  /**
   * @brief First step for a client that launches or resumes while the virtual display is up.
   * @param same_owner The client is the one whose mode the display has (the last to launch or resume).
   * @param mode_matches The display already runs at the client's mode.
   * A different client always gets a new display, so nothing of the previous client's mode carries over.
   */
  constexpr step_e on_connect(bool same_owner, bool mode_matches) {
    if (!same_owner) {
      return step_e::recreate;
    }
    return mode_matches ? step_e::keep : step_e::change_in_place;
  }

  /**
   * @brief Next step after an in-place change, from whether the change held when read back.
   */
  constexpr step_e after_in_place(bool change_held) {
    return change_held ? step_e::keep : step_e::recreate;
  }
}  // namespace display_switch
