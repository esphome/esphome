#ifdef USE_HOST
#include "common.h"

namespace esphome::ld6002b::testing {

namespace {

static constexpr uint32_t CMD_POINT_CLOUD_OFF = 0x07;
static constexpr uint32_t CMD_GET_LOW_POWER = 0x18;
// A report type the base does not know, standing in for a model's own work mode report
static constexpr uint16_t TYPE_REPORT_MODEL_MODE = 0x0F00;

// A model whose firmware reports the work mode itself, as the LD6004 does.
class ModelReportedWorkMode : public ld600x::LD600XComponent {
 public:
  ModelReportedWorkMode() : LD600XComponent(LOG_STR("ld600x_test")) {}

 protected:
  bool work_mode_uses_fallback() const override { return false; }
  bool handle_model_report(uint16_t type, const uint8_t *data, uint16_t len) override {
    if (type != TYPE_REPORT_MODEL_MODE || len < 1)
      return false;
    this->work_mode_text_sensor_->publish_state(data[0] != 0 ? "tracking" : "idle");
    return true;
  }
};

/// A hub with only the work mode text sensor and the low power switch, past its boot timeout.
template<typename T> class WorkModeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->temp_dir_ = std::filesystem::temp_directory_path() / "esphome_ld600x_work_mode_test";
    std::filesystem::remove_all(this->temp_dir_);
    std::filesystem::create_directories(this->temp_dir_);
    setenv("ESPHOME_PREFDIR", this->temp_dir_.c_str(), 1);
    host::host_preferences = &this->prefs_;
    global_preferences = &this->prefs_;

    this->sut_.set_uart_parent(&this->uart_);
    this->sut_.set_work_mode_text_sensor(&this->work_mode_);
    this->sut_.setup();
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    App.scheduler.call(millis());
  }

  void TearDown() override {
    host::setup_preferences();
    unsetenv("ESPHOME_PREFDIR");
    std::filesystem::remove_all(this->temp_dir_);
  }

  void feed_(const std::vector<uint8_t> &bytes) {
    this->uart_.rx.insert(this->uart_.rx.end(), bytes.begin(), bytes.end());
    this->sut_.loop();
  }

  host::HostPreferences prefs_;
  std::filesystem::path temp_dir_;
  MockUARTComponent uart_;
  T sut_;
  text_sensor::TextSensor work_mode_;
};

using FallbackWorkModeTest = WorkModeTest<LD6002BComponent>;
using ModelWorkModeTest = WorkModeTest<ModelReportedWorkMode>;

}  // namespace

TEST_F(FallbackWorkModeTest, BootStartsTargetStreamAndQueriesLowPower) {
  EXPECT_EQ(this->uart_.tx, command(FIRST_COMMAND_ID, TYPE_CONTROL, u32_payload(CMD_TARGET_DISPLAY_ON)));
  this->uart_.tx.clear();
  this->feed_(ack(TYPE_CONTROL));
  this->uart_.tx.clear();
  this->feed_(ack(TYPE_CONTROL));
  EXPECT_EQ(this->uart_.tx, command(FIRST_COMMAND_ID + 2, TYPE_CONTROL, u32_payload(CMD_GET_LOW_POWER)));
}

TEST_F(ModelWorkModeTest, BootSkipsTargetStreamAndLowPowerQuery) {
  // Only the point cloud stream is switched off; nothing is queried for the work mode.
  EXPECT_EQ(this->uart_.tx, command(FIRST_COMMAND_ID, TYPE_CONTROL, u32_payload(CMD_POINT_CLOUD_OFF)));
  this->uart_.tx.clear();
  this->feed_(ack(TYPE_CONTROL));
  EXPECT_TRUE(this->uart_.tx.empty());
}

TEST_F(ModelWorkModeTest, LowPowerAndPresenceDoNotOverwriteModelText) {
  this->feed_(tf_frame(TYPE_REPORT_MODEL_MODE, {0x01}));
  EXPECT_EQ(this->work_mode_.get_state(), "tracking");
  this->feed_(tf_frame(TYPE_REPORT_LOW_POWER, {0x01}));
  EXPECT_EQ(this->work_mode_.get_state(), "tracking");
  this->feed_(target_report({{1.0f, 1.0f, 1.0f, 0, 4}}));
  this->feed_(target_report({}));
  EXPECT_EQ(this->work_mode_.get_state(), "tracking");
  this->feed_(tf_frame(TYPE_REPORT_MODEL_MODE, {0x00}));
  EXPECT_EQ(this->work_mode_.get_state(), "idle");
}

TEST_F(ModelWorkModeTest, LowPowerReportAloneLeavesWorkModeUnset) {
  this->feed_(tf_frame(TYPE_REPORT_LOW_POWER, {0x01}));
  EXPECT_FALSE(this->work_mode_.has_state());
}

}  // namespace esphome::ld6002b::testing

#endif  // USE_HOST
