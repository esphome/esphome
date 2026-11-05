#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "esphome/components/http_request/ota/ota_http_request.h"
#include "esphome/components/http_request/update/http_request_update.h"
#include "esphome/components/json/json_util.h"

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

JsonDocument parse(const std::string &json) {
  JsonDocument doc;
  deserializeJson(doc, json);
  return doc;
}

}  // namespace

// Friend of OtaHttpRequestComponent, so OTA state can be reported without a flash.
class OtaHttpRequestTest : public ::testing::Test {
 protected:
  static void report_state(OtaHttpRequestComponent &ota, ota::OTAState state, uint8_t error) {
    ota.report_state_(state, 50.0f, error);
  }
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

TEST_F(OtaHttpRequestTest, MatchesOnlyBuildsForThisChipFamily) {
  OtaHttpRequestComponent ota;
  EXPECT_STREQ(ota.get_manifest_key(), "ota");

  JsonDocument matching = parse(std::string(R"({"chipFamily": ")") + ESPHOME_VARIANT + R"("})");
  EXPECT_TRUE(ota.matches_build(matching.as<JsonObjectConst>()));

  JsonDocument other_family = parse(R"({"chipFamily": "NOT-A-CHIP"})");
  EXPECT_FALSE(ota.matches_build(other_family.as<JsonObjectConst>()));

  JsonDocument not_a_string = parse(R"({"chipFamily": 42})");
  EXPECT_FALSE(ota.matches_build(not_a_string.as<JsonObjectConst>()));

  JsonDocument absent = parse(R"({"ota": {}})");
  EXPECT_FALSE(ota.matches_build(absent.as<JsonObjectConst>()));
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

}  // namespace esphome::http_request::testing
