#ifdef USE_HOST
#include "../common.h"

#include "esphome/components/select/select.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::ld6004::testing {

namespace {

constexpr uint16_t TYPE_REPORT_P20_MODE = 0x0A15;

struct ModeCommand {
  const char *option;
  uint32_t command;
};

// The select options in the order select/__init__.py sends them, with the control code each one must send.
constexpr ModeCommand WORK_MODES[] = {
    {"normal", 0x17},
    {"low_power", 0x16},
    {"radar_off_p20_high", 0x1B},
    {"radar_off_p20_low", 0x1C},
    {"high_reflectivity", 0x24},
};
constexpr ModeCommand P20_MODES[] = {
    {"presence_high", 0x1D}, {"presence_low", 0x1E}, {"constant_low", 0x20},
    {"constant_high", 0x21}, {"pulse_low", 0x22},    {"pulse_high", 0x23},
};

template<size_t N> void set_options(select::Select &entity, const ModeCommand (&modes)[N]) {
  FixedVector<const char *> options;
  options.init(N);
  for (const auto &mode : modes)
    options.push_back(mode.option);
  entity.traits.set_options(options);
}

}  // namespace

class LD6004SelectTest : public LD6004Test {
 protected:
  void SetUp() override {
    set_options(this->work_mode_, WORK_MODES);
    set_options(this->p20_mode_, P20_MODES);
    this->sut_.set_work_mode_select(&this->work_mode_);
    this->sut_.set_p20_mode_select(&this->p20_mode_);
    this->sut_.set_work_mode_text_sensor(&this->work_mode_text_);
    LD6004Test::SetUp();
    // Each test starts with an idle queue
    this->drain_commands_();
  }

  /// The control code of the command on the wire, which is then ACKed.
  uint32_t take_control_() {
    EXPECT_GE(this->uart_.tx.size(), ld600x::TF_PAYLOAD_OFFSET + 4);
    if (this->uart_.tx.size() < ld600x::TF_PAYLOAD_OFFSET + 4)
      return 0;
    EXPECT_EQ(encode_uint16(this->uart_.tx[5], this->uart_.tx[6]), TYPE_CONTROL);
    const uint32_t control = encode_value_le<uint32_t>(&this->uart_.tx[ld600x::TF_PAYLOAD_OFFSET]);
    this->uart_.tx.clear();
    this->feed_(ack(TYPE_CONTROL));
    return control;
  }

  /// Every option sends its own control code and is then shown.
  template<size_t N> void expect_options_send_commands_(select::Select &entity, const ModeCommand (&modes)[N]) {
    ASSERT_TRUE(this->uart_.tx.empty());
    for (const auto &mode : modes) {
      entity.make_call().set_option(mode.option).perform();
      EXPECT_EQ(this->take_control_(), mode.command) << mode.option;
      EXPECT_EQ(entity.current_option(), mode.option);
    }
  }

  TestSelect work_mode_;
  TestSelect p20_mode_;
  text_sensor::TextSensor work_mode_text_;
};

TEST_F(LD6004SelectTest, WorkModeOptionsSendTheirCommands) {
  this->expect_options_send_commands_(this->work_mode_, WORK_MODES);
}

TEST_F(LD6004SelectTest, P20ModeOptionsSendTheirCommands) {
  this->expect_options_send_commands_(this->p20_mode_, P20_MODES);
}

// The select and the text sensor read the same 0x0A12 value as the same name
TEST_F(LD6004SelectTest, WorkModeReportPublishesMatchingOption) {
  for (uint8_t i = 0; i < WORK_MODE_COUNT; i++) {
    this->feed_(work_mode(i));
    EXPECT_EQ(this->work_mode_.current_option(), WORK_MODES[i].option);
    EXPECT_EQ(this->work_mode_text_.state, WORK_MODES[i].option);
  }
}

// The module may send the P20 mode as one byte or as a uint32, like the work mode
TEST_F(LD6004SelectTest, P20ModeReportPublishesOption) {
  for (bool wide : {false, true}) {
    for (uint8_t i = 0; i < std::size(P20_MODES); i++) {
      this->feed_(wide ? tf_frame(TYPE_REPORT_P20_MODE, u32_payload(i)) : tf_frame(TYPE_REPORT_P20_MODE, {i}));
      EXPECT_EQ(this->p20_mode_.current_option(), P20_MODES[i].option) << (wide ? "as uint32" : "as byte");
    }
  }
}

TEST_F(LD6004SelectTest, MalformedP20ModeReportIsIgnored) {
  this->feed_(tf_frame(TYPE_REPORT_P20_MODE, {2}));
  this->feed_(tf_frame(TYPE_REPORT_P20_MODE, {6}));
  this->feed_(tf_frame(TYPE_REPORT_P20_MODE, u32_payload(6)));
  this->feed_(tf_frame(TYPE_REPORT_P20_MODE, {1, 0}));
  EXPECT_EQ(this->p20_mode_.current_option(), "constant_low");
}

// A full command queue drops the write, and the select keeps showing the module's mode
TEST_F(LD6004SelectTest, WorkModeKeepsOptionWhenQueueFull) {
  this->feed_(work_mode(0));
  this->fill_command_queue_([this] { return this->sut_.set_select_value(&this->p20_mode_, 0); });
  this->work_mode_.make_call().set_option("low_power").perform();
  EXPECT_EQ(this->work_mode_.current_option(), "normal");
}

}  // namespace esphome::ld6004::testing

#endif  // USE_HOST
