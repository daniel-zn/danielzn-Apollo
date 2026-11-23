/**
 * @file tests/unit/platform/test_clipboard_windows.cpp
 * @brief Test the Windows clipboard in src/platform/windows/misc.cpp.
 *
 * It replaces the clipboard of the window station it runs in: run it as SYSTEM in
 * session 0 (`PsExec64 -s -i 0 test_sunshine.exe --gtest_filter=Clipboard*`), whose
 * clipboard nobody uses, never in a signed-in user's session.
 */
#ifdef _WIN32
  #include "../../tests_common.h"

  #include <src/platform/common.h>
  #include <thread>
  #include <Windows.h>

// The HTTPS thread that serves /actions/clipboard also launches streams, and
// syncThreadDesktop() leaves it on the input desktop of that moment: Winlogon when
// the stream starts at the sign-in or lock screen. The clipboard must still work.
TEST(ClipboardTests, WorksFromAThreadLeftOnTheSignInDesktop) {
  bool moved = false;
  bool set = false;
  std::string read;

  std::thread([&]() {
    // Only SYSTEM may open Winlogon
    HDESK winlogon = OpenDesktopW(L"Winlogon", 0, FALSE, GENERIC_ALL);
    moved = winlogon != nullptr && SetThreadDesktop(winlogon);
    if (!moved) {
      return;
    }
    set = platf::set_clipboard("first line\nsecond line \xc3\xa9");
    read = platf::get_clipboard();
  }).join();

  if (!moved) {
    GTEST_SKIP() << "needs SYSTEM in session 0 (PsExec64 -s -i 0)";
  }
  EXPECT_TRUE(set);
  EXPECT_EQ(read, "first line\r\nsecond line \xc3\xa9");
}
#endif
