/**
 * @file src/lock_after_stream.cpp
 * @brief Locks the PC a set time after the last stream ends.
 */
// standard includes
#include <thread>

// local includes
#include "config.h"
#include "globals.h"
#include "lock_after_stream.h"
#include "logging.h"
#include "rtsp.h"

#ifdef _WIN32
  // lib includes
  #include <boost/filesystem.hpp>

  // local includes
  #include "platform/common.h"
  #include "platform/windows/utils.h"
  #include "process.h"
#endif

using namespace std::literals;

namespace lock_after_stream {
#ifdef _WIN32
  /**
   * @brief When the console session last had keyboard or mouse input.
   */
  static clock::time_point last_input(clock::time_point now) {
    LASTINPUTINFO info {sizeof(info)};
    if (!GetLastInputInfo(&info)) {
      return now;  // can't tell: treat it as input, so this check doesn't lock under someone
    }
    return now - std::chrono::milliseconds(GetTickCount() - info.dwTime);
  }

  void lock(const char *reason) {
    if (is_user_session_locked()) {
      BOOST_LOG(info) << reason << ": the session is already locked"sv;
      return;
    }
    // LockWorkStation works only from the user's own session, so it runs there like apps and commands do.
    std::error_code ec;
    boost::process::v1::environment env = proc::proc.get_env();
    boost::filesystem::path dir;
    auto child = platf::run_command(false, true, "rundll32.exe user32.dll,LockWorkStation", dir, env, nullptr, ec, nullptr);
    if (ec) {
      BOOST_LOG(info) << reason << ": not locked, no signed-in session ("sv << ec.message() << ')';
      return;
    }
    child.wait();
    BOOST_LOG(info) << reason << ": locked the PC"sv;
  }

  void run() {
    auto shutdown_event = mail::man->event<bool>(mail::shutdown);
    timer_t timer;
    while (!shutdown_event->peek()) {
      const auto now = clock::now();
      if (timer.update(config::sunshine.lock_after_stream, rtsp_stream::session_count() > 0, now, last_input(now))) {
        lock("Lock after stream");
      }
      std::this_thread::sleep_for(1s);
    }
  }
#else
  void lock(const char *) {}

  void run() {}
#endif
}  // namespace lock_after_stream
