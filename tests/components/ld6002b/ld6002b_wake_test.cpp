#ifdef USE_HOST
#include "common.h"

namespace esphome::ld6002b::testing {

namespace {

static constexpr uint32_t PULSE_MS = 20;

// A wake-up pin that only records its level.
class FakePin final : public GPIOPin {
 public:
  void setup() override {}
  void pin_mode(gpio::Flags flags) override {}
  gpio::Flags get_flags() const override { return gpio::FLAG_OUTPUT; }
  bool digital_read() override { return this->level; }
  void digital_write(bool value) override { this->level = value; }
  bool level{false};
};

}  // namespace

/// The hub with a wake-up pin, so every command waits for its wake pulse before it is written.
class LD6002BWakeTest : public LD6002BTest {
 protected:
  void SetUp() override {
    this->sut_.set_wakeup_pin(&this->wake_pin_);
    this->sut_.set_wakeup_pulse_ms(PULSE_MS);
    this->configure_();
    LD6002BTest::SetUp();
  }

  virtual void configure_() {}

  /// Run the scheduler past the pending wake pulse.
  void finish_pulse_() {
    std::this_thread::sleep_for(std::chrono::milliseconds(PULSE_MS + 10));
    App.scheduler.call(millis());
  }

  /// ACK every boot command in turn, letting each wake pulse finish; returns the id the next command gets.
  uint16_t drain_pulsed_commands_() {
    uint16_t next_id = FIRST_COMMAND_ID;
    for (int i = 0; i < 32; i++) {
      this->finish_pulse_();
      if (this->uart_.tx.size() < ld600x::TF_FRAME_OVERHEAD)
        break;
      const uint16_t type = encode_uint16(this->uart_.tx[5], this->uart_.tx[6]);
      this->uart_.tx.clear();
      this->feed_(ack(type));
      next_id++;
    }
    EXPECT_TRUE(this->uart_.tx.empty());
    return next_id;
  }

  FakePin wake_pin_;
};

class LD6002BNoAutoWakeTest : public LD6002BWakeTest {
 protected:
  void configure_() override { this->sut_.set_auto_wake(false); }
};

TEST_F(LD6002BWakeTest, FirstCommandWaitsForWakePulse) {
  EXPECT_TRUE(this->uart_.tx.empty());
  EXPECT_FALSE(this->wake_pin_.level);
  this->finish_pulse_();
  EXPECT_TRUE(this->wake_pin_.level);
  EXPECT_EQ(this->uart_.tx, command(FIRST_COMMAND_ID, TYPE_CONTROL, u32_payload(CMD_TARGET_DISPLAY_ON)));
}

TEST_F(LD6002BWakeTest, NextQueuedCommandGetsItsOwnPulse) {
  this->finish_pulse_();
  this->uart_.tx.clear();
  this->feed_(ack(TYPE_CONTROL));
  EXPECT_TRUE(this->uart_.tx.empty());
  EXPECT_FALSE(this->wake_pin_.level);
  this->finish_pulse_();
  EXPECT_TRUE(this->wake_pin_.level);
  EXPECT_EQ(this->uart_.tx, command(FIRST_COMMAND_ID + 1, TYPE_CONTROL, u32_payload(CMD_POINT_CLOUD_ON)));
}

// The deferred write carries the command's own type, length and payload
TEST_F(LD6002BWakeTest, DeferredWriteKeepsTypeAndPayload) {
  this->feed_(tf_frame(TYPE_REPORT_Z_RANGE, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40}));
  const uint16_t id = this->drain_pulsed_commands_();
  this->z_max_.make_call().set_value(2.5f).perform();
  EXPECT_TRUE(this->uart_.tx.empty());
  this->finish_pulse_();
  // 0.0f and 2.5f (0x40200000)
  EXPECT_EQ(this->uart_.tx, command(id, TYPE_SET_Z_RANGE, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x40}));
}

TEST_F(LD6002BWakeTest, UnansweredDeferredCommandIsResentAfterAnotherPulse) {
  this->finish_pulse_();
  this->uart_.tx.clear();
  // The first attempt waits 600 ms, since a sleeping module only wakes on it
  LoopBlockingGuard clock{nullptr, nullptr, App.get_loop_component_start_time() + 650};
  this->feed_({});
  EXPECT_TRUE(this->uart_.tx.empty());
  EXPECT_FALSE(this->wake_pin_.level);
  this->finish_pulse_();
  EXPECT_EQ(this->uart_.tx, command(FIRST_COMMAND_ID + 1, TYPE_CONTROL, u32_payload(CMD_TARGET_DISPLAY_ON)));
}

TEST_F(LD6002BNoAutoWakeTest, CommandIsWrittenWithoutPulse) {
  EXPECT_TRUE(this->wake_pin_.level);
  EXPECT_EQ(this->uart_.tx, command(FIRST_COMMAND_ID, TYPE_CONTROL, u32_payload(CMD_TARGET_DISPLAY_ON)));
}

}  // namespace esphome::ld6002b::testing

#endif  // USE_HOST
