/**
 * @file src/display_scale.cpp
 * @brief Definitions for per-client Windows display scale steps.
 */
// standard includes
#include <algorithm>
#include <cstdlib>

// local includes
#include "display_scale.h"

namespace display_scale {
  namespace {
    constexpr int last_index = static_cast<int>(steps.size()) - 1;

    int recommended_index(const state_t &state) {
      return std::clamp(-state.min_rel, 0, last_index);
    }

    int index_of(int percent) {
      int best = 0;
      for (int i = 1; i <= last_index; ++i) {
        if (std::abs(steps[i] - percent) < std::abs(steps[best] - percent)) {
          best = i;
        }
      }
      return best;
    }
  }  // namespace

  int normalize(int percent) {
    return percent <= auto_scale ? auto_scale : steps[index_of(percent)];
  }

  int recommended(const state_t &state) {
    return steps[recommended_index(state)];
  }

  int current(const state_t &state) {
    return steps[std::clamp(recommended_index(state) + state.cur_rel, 0, last_index)];
  }

  int to_store(const state_t &state) {
    int now = current(state);
    return now == recommended(state) ? auto_scale : now;
  }

  plan_t plan(const state_t &state, int stored) {
    int rec = recommended_index(state);
    stored = normalize(stored);
    int want = stored == auto_scale ? rec : index_of(stored);
    int lowest = std::clamp(rec + state.min_rel, 0, last_index);
    int highest = std::clamp(rec + state.max_rel, lowest, last_index);
    int index = std::clamp(want, lowest, highest);
    return {index - rec, steps[index], index != want};
  }

  int remembered(const state_t &state, int stored) {
    return plan(state, stored).rel == state.cur_rel ? normalize(stored) : to_store(state);
  }
}  // namespace display_scale
