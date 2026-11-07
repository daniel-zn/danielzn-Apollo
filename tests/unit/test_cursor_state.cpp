/**
 * @file tests/unit/test_cursor_state.cpp
 * @brief Test src/cursor_state.*
 */
#include "../tests_common.h"

#include <src/cursor_state.h>

using namespace std::chrono_literals;
using stream::cursor::detector_t;
using stream::cursor::sample_t;

namespace {
  sample_t desktop() {
    sample_t s;
    s.valid = true;
    s.visible = true;
    return s;
  }

  sample_t game() {
    sample_t s = desktop();
    s.visible = false;
    return s;
  }

}  // namespace

TEST(CursorStateTests, FirstSampleCountsAtOnce) {
  auto t = detector_t::clock::now();
  detector_t a, b;
  EXPECT_FALSE(a.update(desktop(), t));
  EXPECT_TRUE(b.update(game(), t));
}

TEST(CursorStateTests, HiddenCursorIsCapturedAfterSettling) {
  auto t = detector_t::clock::now();
  detector_t d;
  EXPECT_FALSE(d.update(desktop(), t));

  auto g = game();
  g.mouse_events = 1;  // the click that started the game
  EXPECT_FALSE(d.update(g, t + 100ms));  // not settled yet
  EXPECT_FALSE(d.update(g, t + 200ms));
  EXPECT_TRUE(d.update(g, t + 360ms));

  // Shows again (a menu): free after settling
  auto m = desktop();
  m.mouse_events = 1;
  EXPECT_TRUE(d.update(m, t + 400ms));
  EXPECT_FALSE(d.update(m, t + 700ms));
}

TEST(CursorStateTests, FlickerIsIgnored) {
  auto t = detector_t::clock::now();
  detector_t d;
  d.update(desktop(), t);
  EXPECT_FALSE(d.update(game(), t + 100ms));
  EXPECT_FALSE(d.update(desktop(), t + 200ms));  // back before settling
  EXPECT_FALSE(d.update(game(), t + 300ms));  // the timer starts over
  EXPECT_FALSE(d.update(game(), t + 500ms));
  EXPECT_TRUE(d.update(game(), t + 560ms));
}

TEST(CursorStateTests, ClippedVisibleCursorIsCaptured) {
  auto t = detector_t::clock::now();
  detector_t d;
  auto s = desktop();
  s.clipped = true;
  EXPECT_TRUE(d.update(s, t));
}

TEST(CursorStateTests, UnreadableCursorIsFree) {
  auto t = detector_t::clock::now();
  detector_t d;
  EXPECT_TRUE(d.update(game(), t));
  sample_t secure;  // valid = false, e.g. a UAC prompt
  d.update(secure, t + 100ms);
  EXPECT_FALSE(d.update(secure, t + 400ms));
}

TEST(CursorStateTests, HidePointerWhileTypingStaysFree) {
  auto t = detector_t::clock::now();
  detector_t d;
  auto typing = desktop();
  typing.hides_while_typing = true;
  typing.mouse_events = 5;
  d.update(typing, t);

  // A key, then Windows hides the pointer
  typing.last_input_keyboard = true;
  typing.visible = false;
  EXPECT_FALSE(d.update(typing, t + 100ms));
  EXPECT_FALSE(d.update(typing, t + 1s));
  EXPECT_FALSE(d.update(typing, t + 5s));

  // The mouse moves: the pointer shows again
  typing.last_input_keyboard = false;
  typing.mouse_events = 6;
  typing.visible = true;
  EXPECT_FALSE(d.update(typing, t + 6s));
}

TEST(CursorStateTests, KeyboardIntoAGameIsCapturedOnceTheMouseMoves) {
  auto t = detector_t::clock::now();
  detector_t d;
  auto s = desktop();
  s.hides_while_typing = true;
  d.update(s, t);

  // Alt+Tab into a game that hides the cursor: looks like typing until the mouse moves
  s.last_input_keyboard = true;
  s.visible = false;
  EXPECT_FALSE(d.update(s, t + 100ms));
  EXPECT_FALSE(d.update(s, t + 1s));

  // The mouse moves and the cursor stays hidden: a game has it
  s.last_input_keyboard = false;
  s.mouse_events = 1;
  EXPECT_FALSE(d.update(s, t + 1100ms));
  EXPECT_TRUE(d.update(s, t + 1400ms));
}

TEST(CursorStateTests, KeysInAGameKeepItCaptured) {
  auto t = detector_t::clock::now();
  detector_t d;
  auto s = game();
  s.hides_while_typing = true;
  EXPECT_TRUE(d.update(s, t));
  s.last_input_keyboard = true;  // walking with W
  EXPECT_TRUE(d.update(s, t + 100ms));
  EXPECT_TRUE(d.update(s, t + 2s));
}

TEST(CursorStateTests, WithoutHideWhileTypingAHiddenCursorIsCaptured) {
  auto t = detector_t::clock::now();
  detector_t d;
  auto s = desktop();
  d.update(s, t);
  s.last_input_keyboard = true;
  s.visible = false;
  d.update(s, t + 100ms);
  EXPECT_TRUE(d.update(s, t + 400ms));
}

TEST(CursorStateTests, ResetForgetsTheState) {
  auto t = detector_t::clock::now();
  detector_t d;
  EXPECT_TRUE(d.update(game(), t));
  d.reset();
  EXPECT_FALSE(d.captured());
  EXPECT_FALSE(d.update(desktop(), t + 10ms));
}

TEST(CursorStateTests, EncodesTheControlMessage) {
  auto captured = stream::cursor::encode_message(true);
  auto free = stream::cursor::encode_message(false);
  const std::array<std::uint8_t, 6> want_captured {0x80, 0x30, 0x02, 0x00, 0x01, 0x01};
  const std::array<std::uint8_t, 6> want_free {0x80, 0x30, 0x02, 0x00, 0x01, 0x00};
  EXPECT_EQ(captured, want_captured);
  EXPECT_EQ(free, want_free);
  EXPECT_EQ(stream::cursor::SDP_ATTRIBUTE, "a=x-apollo-cursor:1");
}
