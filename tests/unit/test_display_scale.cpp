/**
 * @file tests/unit/test_display_scale.cpp
 * @brief Test src/display_scale.* and the per-client display scale in the state file (src/nvhttp.cpp).
 */
#include "../tests_common.h"

#include <filesystem>
#include <fstream>

#include <src/config.h>
#include <src/crypto.h>
#include <src/display_scale.h>
#include <src/nvhttp.h>

using display_scale::state_t;

// Windows reports steps relative to the recommended scale; 100% is always the lowest step.
// recommended 100%, allowed 100..175% (a 1920x1080 display)
constexpr state_t at_1080p {0, 0, 3};
// recommended 150%, allowed 100..300% (a 3840x2160 display), currently 200%
constexpr state_t at_4k_200 {-2, 2, 5};

TEST(DisplayScaleTest, NormalizeKeepsStepsAndAuto) {
  EXPECT_EQ(display_scale::normalize(0), display_scale::auto_scale);
  EXPECT_EQ(display_scale::normalize(-25), display_scale::auto_scale);
  for (int step : display_scale::steps) {
    EXPECT_EQ(display_scale::normalize(step), step);
  }
}

TEST(DisplayScaleTest, NormalizeRoundsToTheNearestStep) {
  EXPECT_EQ(display_scale::normalize(1), 100);
  EXPECT_EQ(display_scale::normalize(130), 125);
  EXPECT_EQ(display_scale::normalize(140), 150);
  EXPECT_EQ(display_scale::normalize(260), 250);
  EXPECT_EQ(display_scale::normalize(280), 300);
  EXPECT_EQ(display_scale::normalize(900), 500);
}

TEST(DisplayScaleTest, ReadsCurrentAndRecommended) {
  EXPECT_EQ(display_scale::recommended(at_1080p), 100);
  EXPECT_EQ(display_scale::current(at_1080p), 100);
  EXPECT_EQ(display_scale::recommended(at_4k_200), 150);
  EXPECT_EQ(display_scale::current(at_4k_200), 200);
}

TEST(DisplayScaleTest, StoresAutoAtTheRecommendedScale) {
  EXPECT_EQ(display_scale::to_store(at_1080p), display_scale::auto_scale);
  EXPECT_EQ(display_scale::to_store(state_t {-2, 0, 5}), display_scale::auto_scale);
  EXPECT_EQ(display_scale::to_store(at_4k_200), 200);
  // 100% on a display that recommends 150% is a choice, not Auto
  EXPECT_EQ(display_scale::to_store(state_t {-2, -2, 5}), 100);
}

TEST(DisplayScaleTest, PlansStoredScaleRelativeToRecommended) {
  auto p = display_scale::plan(at_4k_200, 250);
  EXPECT_EQ(p.rel, 4);
  EXPECT_EQ(p.percent, 250);
  EXPECT_FALSE(p.clamped);

  p = display_scale::plan(at_4k_200, 100);
  EXPECT_EQ(p.rel, -2);
  EXPECT_EQ(p.percent, 100);
  EXPECT_FALSE(p.clamped);
}

TEST(DisplayScaleTest, AutoPlansTheRecommendedScale) {
  auto p = display_scale::plan(at_4k_200, display_scale::auto_scale);
  EXPECT_EQ(p.rel, 0);
  EXPECT_EQ(p.percent, 150);
  EXPECT_FALSE(p.clamped);
}

TEST(DisplayScaleTest, ClampsToTheAllowedRange) {
  // 200% isn't offered at 1080p (100..175%): use 175%
  auto p = display_scale::plan(at_1080p, 200);
  EXPECT_EQ(p.rel, 3);
  EXPECT_EQ(p.percent, 175);
  EXPECT_TRUE(p.clamped);

  // 500% on a display that allows up to 300%
  p = display_scale::plan(at_4k_200, 500);
  EXPECT_EQ(p.percent, 300);
  EXPECT_EQ(p.rel, 5);
  EXPECT_TRUE(p.clamped);

  // A display that only allows 100%
  p = display_scale::plan(state_t {0, 0, 0}, 150);
  EXPECT_EQ(p.rel, 0);
  EXPECT_EQ(p.percent, 100);
  EXPECT_TRUE(p.clamped);
}

TEST(DisplayScaleTest, OddStoredValuesPlanTheNearestStep) {
  auto p = display_scale::plan(at_4k_200, 130);
  EXPECT_EQ(p.percent, 125);
  EXPECT_EQ(p.rel, -1);
}

TEST(DisplayScaleTest, OutOfRangeReportsStayInsideTheStepList) {
  auto p = display_scale::plan(state_t {-20, 30, 40}, 500);
  EXPECT_EQ(p.percent, 500);
  EXPECT_EQ(display_scale::current(state_t {-20, 30, 40}), 500);
  EXPECT_EQ(display_scale::recommended(state_t {5, 0, 0}), 100);
}

TEST(DisplayScaleTest, RememberedKeepsAClampedStoredScale) {
  // Stored 200%, clamped to 175% at 1080p, untouched by the user: keep 200% for a bigger resolution later
  EXPECT_EQ(display_scale::remembered(state_t {0, 3, 3}, 200), 200);
  // The user picked 125% during the session
  EXPECT_EQ(display_scale::remembered(state_t {0, 1, 3}, 200), 125);
  // Auto, still at the recommended scale
  EXPECT_EQ(display_scale::remembered(at_1080p, display_scale::auto_scale), display_scale::auto_scale);
  // Auto, the user picked 200%
  EXPECT_EQ(display_scale::remembered(at_4k_200, display_scale::auto_scale), 200);
  // Stored 200%, the user went back to the recommended scale: Auto again
  EXPECT_EQ(display_scale::remembered(state_t {-2, 0, 5}, 200), display_scale::auto_scale);
}

namespace {
  struct StateFileTest: testing::Test {
    std::filesystem::path dir;
    std::string saved_file_state;

    void SetUp() override {
      dir = std::filesystem::temp_directory_path() / ("apollo-display-scale-" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + '-' +
                                                     ::testing::UnitTest::GetInstance()->current_test_info()->name());
      std::filesystem::remove_all(dir);
      std::filesystem::create_directories(dir);
      saved_file_state = config::nvhttp.file_state;
      config::nvhttp.file_state = (dir / "state.json").string();
    }

    void TearDown() override {
      // Leave no clients behind for other tests
      write(nlohmann::json::array());
      nvhttp::load_state();
      config::nvhttp.file_state = saved_file_state;
      std::filesystem::remove_all(dir);
    }

    static nlohmann::json device(const std::string &name, const std::string &uuid, const std::string &cert) {
      return {{"name", name}, {"uuid", uuid}, {"cert", cert}, {"perm", 119480064}, {"display_mode", ""}};
    }

    void write(const nlohmann::json &devices) const {
      nlohmann::json root;
      root["root"]["uniqueid"] = "4D494E49-5445-5354-0000-000000000000";
      root["root"]["named_devices"] = devices;
      std::ofstream(config::nvhttp.file_state) << root.dump();
    }

    nlohmann::json read_back() const {
      nlohmann::json j;
      std::ifstream(config::nvhttp.file_state) >> j;
      return j["root"]["named_devices"];
    }

    static nlohmann::json find(const nlohmann::json &devices, const std::string &uuid) {
      for (auto &d : devices) {
        if (d.value("uuid", "") == uuid) {
          return d;
        }
      }
      return {};
    }
  };
}  // namespace

TEST_F(StateFileTest, LoadsSavesAndUpdatesEachClientsScale) {
  auto phone = crypto::gen_creds("phone", 2048);
  auto mac = crypto::gen_creds("mac", 2048);
  auto old = crypto::gen_creds("old", 2048);

  auto phone_entry = device("Phone", "AAAAAAAA-0000-0000-0000-000000000001", phone.x509);
  phone_entry["display_scale"] = 200;
  auto mac_entry = device("Mac", "AAAAAAAA-0000-0000-0000-000000000002", mac.x509);
  mac_entry["display_scale"] = "130";  // odd value, as a string: nearest step
  auto old_entry = device("Old", "AAAAAAAA-0000-0000-0000-000000000003", old.x509);  // no key: Auto
  write(nlohmann::json::array({phone_entry, mac_entry, old_entry}));

  nvhttp::load_state();
  EXPECT_EQ(nvhttp::get_client_display_scale("AAAAAAAA-0000-0000-0000-000000000001"), 200);
  EXPECT_EQ(nvhttp::get_client_display_scale("AAAAAAAA-0000-0000-0000-000000000002"), 125);
  EXPECT_EQ(nvhttp::get_client_display_scale("AAAAAAAA-0000-0000-0000-000000000003"), display_scale::auto_scale);
  EXPECT_EQ(nvhttp::get_client_display_scale("unknown"), std::nullopt);

  // The web UI's client list shows it
  auto clients = nvhttp::get_all_clients();
  ASSERT_EQ(clients.size(), 3);
  EXPECT_EQ(find(clients, "AAAAAAAA-0000-0000-0000-000000000001")["display_scale"], 200);

  // Remembering a new scale writes the file and leaves the other fields alone
  EXPECT_TRUE(nvhttp::set_client_display_scale("AAAAAAAA-0000-0000-0000-000000000003", 175));
  EXPECT_FALSE(nvhttp::set_client_display_scale("unknown", 150));
  auto devices = read_back();
  EXPECT_EQ(find(devices, "AAAAAAAA-0000-0000-0000-000000000003")["display_scale"], 175);
  EXPECT_EQ(find(devices, "AAAAAAAA-0000-0000-0000-000000000003")["perm"], 119480064);
  EXPECT_EQ(find(devices, "AAAAAAAA-0000-0000-0000-000000000001")["display_scale"], 200);
  EXPECT_EQ(find(devices, "AAAAAAAA-0000-0000-0000-000000000002")["display_scale"], 125);

  // Round trip: a restart reads back what was saved
  nvhttp::load_state();
  EXPECT_EQ(nvhttp::get_client_display_scale("AAAAAAAA-0000-0000-0000-000000000003"), 175);

  // The web UI's update keeps the scale when it doesn't send one, and normalizes one it sends
  EXPECT_TRUE(nvhttp::update_device_info("AAAAAAAA-0000-0000-0000-000000000001", "Phone", "", {}, {}, crypto::PERM::_all, true, true, false));
  EXPECT_EQ(nvhttp::get_client_display_scale("AAAAAAAA-0000-0000-0000-000000000001"), 200);
  EXPECT_TRUE(nvhttp::update_device_info("AAAAAAAA-0000-0000-0000-000000000001", "Phone", "", {}, {}, crypto::PERM::_all, true, true, false, 0));
  EXPECT_EQ(nvhttp::get_client_display_scale("AAAAAAAA-0000-0000-0000-000000000001"), display_scale::auto_scale);
  EXPECT_TRUE(nvhttp::update_device_info("AAAAAAAA-0000-0000-0000-000000000001", "Phone", "", {}, {}, crypto::PERM::_all, true, true, false, 240));
  EXPECT_EQ(find(read_back(), "AAAAAAAA-0000-0000-0000-000000000001")["display_scale"], 250);
}
