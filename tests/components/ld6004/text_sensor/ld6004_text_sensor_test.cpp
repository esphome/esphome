#ifdef USE_HOST
#include "../common.h"

#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::ld6004::testing {

namespace {

constexpr uint32_t CMD_GET_WORK_MODE = 0x18;
// SOF, id, length, type and header checksum come before the payload.
constexpr size_t PAYLOAD_OFFSET = ld600x::TF_HEADER_LEN + 2;

}  // namespace

/// Only the work_mode text sensor, which needs the boot query on its own.
class LD6004TextSensorTest : public LD6004Test {
 protected:
  void configure_() override { this->sut_.set_work_mode_text_sensor(&this->work_mode_); }

  /// ACK every boot command in turn and return the control codes that were sent.
  std::vector<uint32_t> drain_controls_() {
    std::vector<uint32_t> controls;
    for (int i = 0; i < 32 && this->uart_.tx.size() >= ld600x::TF_FRAME_OVERHEAD; i++) {
      const uint16_t type = encode_uint16(this->uart_.tx[5], this->uart_.tx[6]);
      if (type == TYPE_CONTROL && this->uart_.tx.size() >= PAYLOAD_OFFSET + 4)
        controls.push_back(encode_value_le<uint32_t>(&this->uart_.tx[PAYLOAD_OFFSET]));
      this->uart_.tx.clear();
      this->feed_(ack(type));
    }
    EXPECT_TRUE(this->uart_.tx.empty());
    return controls;
  }

  text_sensor::TextSensor work_mode_;
};

TEST_F(LD6004TextSensorTest, TextSensorAloneQueriesWorkModeAtBoot) {
  const auto controls = this->drain_controls_();
  EXPECT_EQ(std::count(controls.begin(), controls.end(), CMD_GET_WORK_MODE), 1);
  // The text sensor reads no target reports, so the target stream stays off.
  EXPECT_EQ(std::count(controls.begin(), controls.end(), CMD_TARGET_DISPLAY_ON), 0);
}

// Each 0x0A12 value maps to its own name, sent as one byte or as a uint32
TEST_F(LD6004TextSensorTest, WorkModeReportPublishesEveryMode) {
  static constexpr const char *NAMES[WORK_MODE_COUNT] = {"normal", "low_power", "radar_off_p20_high",
                                                         "radar_off_p20_low", "high_reflectivity"};
  for (bool wide : {false, true}) {
    for (uint8_t mode = 0; mode < WORK_MODE_COUNT; mode++) {
      this->feed_(wide ? tf_frame(TYPE_REPORT_WORK_MODE, u32_payload(mode)) : work_mode(mode));
      EXPECT_EQ(this->work_mode_.state, NAMES[mode]) << "mode " << int(mode) << (wide ? " as uint32" : "");
    }
  }
}

TEST_F(LD6004TextSensorTest, MalformedWorkModeReportDoesNotPublish) {
  this->feed_(work_mode(WORK_MODE_LOW_POWER));
  this->feed_(work_mode(WORK_MODE_COUNT));
  this->feed_(tf_frame(TYPE_REPORT_WORK_MODE, u32_payload(WORK_MODE_COUNT)));
  this->feed_(tf_frame(TYPE_REPORT_WORK_MODE, {WORK_MODE_NORMAL, 0x00}));
  this->feed_(tf_frame(TYPE_REPORT_WORK_MODE, {WORK_MODE_NORMAL, 0x00, 0x00}));
  EXPECT_EQ(this->work_mode_.state, "low_power");
}

}  // namespace esphome::ld6004::testing

#endif  // USE_HOST
