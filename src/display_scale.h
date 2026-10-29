/**
 * @file src/display_scale.h
 * @brief Declarations for per-client Windows display scale steps (see docs/display_scale.md).
 */
#pragma once

// standard includes
#include <array>

namespace display_scale {
  /// Stored value meaning "Windows' recommended scale for the resolution".
  constexpr int auto_scale = 0;

  /// The scale steps Windows offers, in percent, in the order its relative steps count them.
  constexpr std::array<int, 12> steps {100, 125, 150, 175, 200, 225, 250, 300, 350, 400, 450, 500};

  /**
   * @brief A display's scale as Windows reports it: steps relative to the recommended scale.
   * The minimum is always the 100% step, so `-min_rel` is the recommended step's index.
   */
  struct state_t {
    int min_rel;
    int cur_rel;
    int max_rel;
  };

  /**
   * @brief What to set on a display for a stored scale.
   */
  struct plan_t {
    int rel;  ///< Step relative to the recommended scale, for Windows.
    int percent;  ///< The scale that will be set.
    bool clamped;  ///< The stored scale is outside what the display allows at its resolution.
  };

  /**
   * @brief Turn any value into a value that can be stored: Auto (0 or less) or the nearest step.
   */
  int normalize(int percent);

  /**
   * @brief The recommended scale of a display, in percent.
   */
  int recommended(const state_t &state);

  /**
   * @brief The current scale of a display, in percent.
   */
  int current(const state_t &state);

  /**
   * @brief The value to remember for a client: Auto when the display is at the recommended scale.
   */
  int to_store(const state_t &state);

  /**
   * @brief How to give a display a stored scale, clamped to the steps the display allows.
   */
  plan_t plan(const state_t &state, int stored);

  /**
   * @brief The value to remember for a client when its session ends.
   * Keeps the stored value while the display still shows what it was given (so a scale clamped at a small
   * resolution isn't lost), otherwise `to_store()`: the user changed the scale.
   */
  int remembered(const state_t &state, int stored);
}  // namespace display_scale
