#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "esphome/components/http_request/ota/ota_http_request.h"
#include "esphome/components/http_request/update/http_request_update.h"
#include "esphome/components/json/json_util.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::http_request::testing {

namespace {

struct Report {
  UpdateTargetState state;
  float progress;
  const LogString *message;
};

class RecordingListener : public UpdateTargetListener {
 public:
  void on_target_state(UpdateTargetState state, float progress, const LogString *message) override {
    this->reports.push_back({state, progress, message});
  }

  std::vector<Report> reports;
};

// Exposes the report helpers a real target would call from its own install path.
class FakeTarget : public UpdateTarget {
 public:
  bool matches_build(JsonObjectConst build) const override { return build[ESPHOME_F("mcu")].is<const char *>(); }
  void install(const std::string &url, const std::string &md5) override {}

  using UpdateTarget::report_complete_;
  using UpdateTarget::report_failed_;
  using UpdateTarget::report_progress_;
};

// A target that updates something other than this device reports that device's version.
class VersionedTarget : public FakeTarget {
 public:
  const char *get_current_version() const override { return "1.2.3"; }
};

// Asks @p target about a build entry, parsed the same way the update platform parses a
// manifest. json_util owns the document, so nothing here outlives the call.
bool matches_build(const UpdateTarget &target, const std::string &json) {
  bool matched = false;
  const bool parsed =
      json::parse_json(reinterpret_cast<const uint8_t *>(json.data()), json.size(), [&](JsonObject root) {
        matched = target.matches_build(root);
        return true;
      });
  EXPECT_TRUE(parsed);
  return matched;
}

}  // namespace

// Friend of OtaHttpRequestComponent, so OTA state can be reported without a flash.
class OtaHttpRequestTest : public ::testing::Test {
 protected:
  static void report_state(OtaHttpRequestComponent &ota, ota::OTAState state, uint8_t error) {
    ota.report_state_(state, 50.0f, error);
  }
};

// Friend of HttpRequestUpdate, so a finished check can be applied without an HTTP fetch.
class HttpRequestUpdateCheckTest : public ::testing::Test {
 protected:
  static void finish_check(HttpRequestUpdate &update, TaskResult *result) { update.finish_check_(result); }
};

TEST(UpdateTargetReports, ReachTheListenerWithTheirPayload) {
  FakeTarget target;
  RecordingListener listener;
  target.set_update_listener(&listener);

  target.report_progress_(42.0f);
  target.report_complete_();
  target.report_failed_(LOG_STR("went wrong"));

  ASSERT_EQ(listener.reports.size(), 3u);
  EXPECT_EQ(listener.reports[0].state, UpdateTargetState::UPDATE_TARGET_STATE_PROGRESS);
  EXPECT_FLOAT_EQ(listener.reports[0].progress, 42.0f);
  EXPECT_EQ(listener.reports[1].state, UpdateTargetState::UPDATE_TARGET_STATE_COMPLETE);
  EXPECT_EQ(listener.reports[2].state, UpdateTargetState::UPDATE_TARGET_STATE_FAILED);
  EXPECT_STREQ(LOG_STR_ARG(listener.reports[2].message), "went wrong");
}

// A target may be installed from an automation while no update entity points at it.
TEST(UpdateTargetReports, AreDroppedWithoutAListener) {
  FakeTarget target;
  target.report_progress_(42.0f);
  target.report_complete_();
  target.report_failed_(LOG_STR("went wrong"));
}

TEST(UpdateTargetManifest, KeyDefaultsToTheOtaImage) {
  FakeTarget target;
  EXPECT_STREQ(target.get_manifest_key(), "ota");
}

TEST(UpdateTargetManifest, CurrentVersionDefaultsToThisFirmware) {
  FakeTarget target;
#ifdef ESPHOME_PROJECT_VERSION
  EXPECT_STREQ(target.get_current_version(), ESPHOME_PROJECT_VERSION);
#else
  EXPECT_STREQ(target.get_current_version(), ESPHOME_VERSION);
#endif
}

TEST(UpdateTargetManifest, CurrentVersionCanComeFromTheTarget) {
  VersionedTarget target;
  EXPECT_STREQ(target.get_current_version(), "1.2.3");
}

TEST_F(OtaHttpRequestTest, MatchesOnlyBuildsForThisChipFamily) {
  OtaHttpRequestComponent ota;
  EXPECT_STREQ(ota.get_manifest_key(), "ota");

  EXPECT_TRUE(matches_build(ota, std::string(R"({"chipFamily": ")") + ESPHOME_VARIANT + R"("})"));
  EXPECT_FALSE(matches_build(ota, R"({"chipFamily": "NOT-A-CHIP"})"));
  EXPECT_FALSE(matches_build(ota, R"({"chipFamily": 42})"));
  EXPECT_FALSE(matches_build(ota, R"({"ota": {}})"));
}

TEST_F(OtaHttpRequestTest, TranslatesOtaStateForTheUpdateEntity) {
  OtaHttpRequestComponent ota;
  RecordingListener listener;
  ota.set_update_listener(&listener);

  report_state(ota, ota::OTA_STARTED, 0);
  EXPECT_TRUE(listener.reports.empty());

  report_state(ota, ota::OTA_IN_PROGRESS, 0);
  report_state(ota, ota::OTA_COMPLETED, 0);
  report_state(ota, ota::OTA_ABORT, OTA_BAD_URL);
  report_state(ota, ota::OTA_ERROR, OTA_CONNECTION_ERROR);

  ASSERT_EQ(listener.reports.size(), 4u);
  EXPECT_EQ(listener.reports[0].state, UpdateTargetState::UPDATE_TARGET_STATE_PROGRESS);
  EXPECT_FLOAT_EQ(listener.reports[0].progress, 50.0f);
  EXPECT_EQ(listener.reports[1].state, UpdateTargetState::UPDATE_TARGET_STATE_COMPLETE);
  EXPECT_EQ(listener.reports[2].state, UpdateTargetState::UPDATE_TARGET_STATE_FAILED);
  EXPECT_EQ(listener.reports[3].state, UpdateTargetState::UPDATE_TARGET_STATE_FAILED);
}

// install() hands the image to the OTA component, which rejects a URL it cannot fetch.
TEST_F(OtaHttpRequestTest, InstallPassesTheImageToTheOtaComponent) {
  OtaHttpRequestComponent ota;
  ota.install("not-a-url", "0123456789abcdef0123456789abcdef");
  EXPECT_EQ(ota.md5_expected(), "0123456789abcdef0123456789abcdef");
}

TEST(HttpRequestUpdateTargetState, ProgressMovesTheEntityToInstalling) {
  HttpRequestUpdate update;
  update.on_target_state(UpdateTargetState::UPDATE_TARGET_STATE_PROGRESS, 25.0f, nullptr);

  EXPECT_EQ(update.state, update::UPDATE_STATE_INSTALLING);
  EXPECT_TRUE(update.update_info.has_progress);
  EXPECT_FLOAT_EQ(update.update_info.progress, 25.0f);
}

TEST(HttpRequestUpdateTargetState, CompleteAdoptsTheInstalledVersion) {
  HttpRequestUpdate update;
  update.on_target_state(UpdateTargetState::UPDATE_TARGET_STATE_PROGRESS, 25.0f, nullptr);
  update.on_target_state(UpdateTargetState::UPDATE_TARGET_STATE_COMPLETE, 0.0f, nullptr);

  EXPECT_EQ(update.state, update::UPDATE_STATE_NO_UPDATE);
  EXPECT_FALSE(update.update_info.has_progress);
  EXPECT_EQ(update.update_info.current_version, update.update_info.latest_version);
}

TEST(HttpRequestUpdateTargetState, FailureLeavesTheUpdateAvailable) {
  HttpRequestUpdate update;
  update.on_target_state(UpdateTargetState::UPDATE_TARGET_STATE_PROGRESS, 25.0f, nullptr);
  update.on_target_state(UpdateTargetState::UPDATE_TARGET_STATE_FAILED, 0.0f, LOG_STR("went wrong"));

  EXPECT_EQ(update.state, update::UPDATE_STATE_AVAILABLE);
  EXPECT_FALSE(update.update_info.has_progress);
}

TEST_F(HttpRequestUpdateCheckTest, ResultOfAFinishedCheckBecomesTheEntityState) {
  HttpRequestUpdate update;
  auto *result = new TaskResult();
  result->info.latest_version = "2.0.0";
  result->info.current_version = "1.0.0";
  finish_check(update, result);

  EXPECT_EQ(update.state, update::UPDATE_STATE_AVAILABLE);
  EXPECT_EQ(update.update_info.latest_version, "2.0.0");
}

// A target that installs asynchronously is still working when the next check lands; its state
// and the metadata of the image it is installing have to survive.
TEST_F(HttpRequestUpdateCheckTest, ResultIsDiscardedWhileATargetIsInstalling) {
  HttpRequestUpdate update;
  FakeTarget target;
  update.set_target(&target);
  auto *installing = new TaskResult();
  installing->info.latest_version = "2.0.0";
  installing->info.current_version = "1.0.0";
  finish_check(update, installing);
  update.perform(false);
  ASSERT_EQ(update.state, update::UPDATE_STATE_INSTALLING);

  auto *late = new TaskResult();
  late->info.latest_version = "3.0.0";
  late->info.current_version = "1.0.0";
  finish_check(update, late);

  EXPECT_EQ(update.state, update::UPDATE_STATE_INSTALLING);
  EXPECT_EQ(update.update_info.latest_version, "2.0.0");
  // Run the deferred install while update is still alive
  App.scheduler.call(millis());
}

// Without the guard this dereferences the unset request parent and crashes.
TEST_F(HttpRequestUpdateCheckTest, NoCheckIsStartedWhileATargetIsInstalling) {
  HttpRequestUpdate update;
  FakeTarget target;
  update.set_target(&target);
  update.on_target_state(UpdateTargetState::UPDATE_TARGET_STATE_PROGRESS, 10.0f, nullptr);

  update.update();

  EXPECT_EQ(update.state, update::UPDATE_STATE_INSTALLING);
}

}  // namespace esphome::http_request::testing
