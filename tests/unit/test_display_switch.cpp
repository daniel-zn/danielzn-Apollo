/**
 * @file tests/unit/test_display_switch.cpp
 * @brief Test src/display_switch.h: what happens to the virtual display when a client launches or resumes.
 */
#include "../tests_common.h"

#include <array>

#include <src/display_switch.h>

using display_switch::display_mode_t;
using display_switch::step_e;

TEST(DisplaySwitchTest, AnotherClientAlwaysGetsANewDisplay) {
  // Mac (2560x1440@60) owns the display, the phone (2556x1179@120) resumes
  EXPECT_EQ(display_switch::on_connect(false, false), step_e::recreate);
  // Even when the display already runs at the new client's mode
  EXPECT_EQ(display_switch::on_connect(false, true), step_e::recreate);
}

TEST(DisplaySwitchTest, TheSameClientKeepsOrChangesInPlace) {
  EXPECT_EQ(display_switch::on_connect(true, true), step_e::keep);
  EXPECT_EQ(display_switch::on_connect(true, false), step_e::change_in_place);
}

TEST(DisplaySwitchTest, AnInPlaceChangeThatDidNotHoldRecreates) {
  EXPECT_EQ(display_switch::after_in_place(false), step_e::recreate);
  EXPECT_EQ(display_switch::after_in_place(true), step_e::keep);
}

TEST(DisplaySwitchTest, MatchesWithinOneHertz) {
  constexpr display_mode_t phone {2556, 1179, 120};
  EXPECT_TRUE(display_switch::matches(phone, 2556, 1179, 120000));
  EXPECT_TRUE(display_switch::matches({2556, 1179, 119}, 2556, 1179, 119880));
  EXPECT_TRUE(display_switch::matches({1920, 1080, 59}, 1920, 1080, 59940));
  EXPECT_FALSE(display_switch::matches(phone, 2556, 1179, 60000));
  EXPECT_FALSE(display_switch::matches(phone, 2560, 1179, 120000));
  EXPECT_FALSE(display_switch::matches(phone, 2556, 1440, 120000));
}

TEST(DisplaySwitchTest, AChangeHoldsOnlyIfEveryReadingShowsIt) {
  constexpr display_mode_t mac {2560, 1440, 60};
  constexpr display_mode_t phone {2556, 1179, 120};

  constexpr std::array<display_mode_t, 3> stuck {phone, phone, phone};
  EXPECT_TRUE(display_switch::held(stuck, 2556, 1179, 120000));

  // Never took
  constexpr std::array<display_mode_t, 1> ignored {mac};
  EXPECT_FALSE(display_switch::held(ignored, 2556, 1179, 120000));

  // Reported the new mode at first, then fell back to the old one
  constexpr std::array<display_mode_t, 2> fell_back {phone, mac};
  EXPECT_FALSE(display_switch::held(fell_back, 2556, 1179, 120000));

  // Nothing could be read
  EXPECT_FALSE(display_switch::held(std::span<const display_mode_t> {}, 2556, 1179, 120000));
}
