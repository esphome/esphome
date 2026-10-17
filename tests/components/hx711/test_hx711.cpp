#ifdef USE_HOST

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "esphome/components/hx711/hx711.h"
#include "esphome/core/application.h"

namespace esphome::hx711::testing {

static constexpr uint16_t SETTLING_MS = 400;
static constexpr uint16_t TIMEOUT_MS = 2000;

// The chip side of the two wires: DOUT low when a conversion is ready, then 24 data bits on the
// first 24 rising PD_SCK edges; the edges after that select the next conversion.
class FakeChip {
 public:
  void convert(int32_t value) {
    this->value_ = static_cast<uint32_t>(value) & 0xFFFFFF;
    this->edges_ = 0;
    this->ready_ = true;
  }
  bool dout() const {
    if (this->edges_ == 0)
      return !this->ready_;
    if (this->edges_ <= 24)
      return (this->value_ >> (24 - this->edges_)) & 1;
    return true;
  }
  void sck(bool level) {
    if (level && !this->sck_)
      this->edges_++;
    this->sck_ = level;
  }
  bool powered_down() const { return this->sck_; }
  // Pulses after the last read: 1 = A/128, 2 = B/32, 3 = A/64.
  unsigned gain_pulses() const { return this->edges_ > 24 ? this->edges_ - 24 : 0; }

 protected:
  uint32_t value_{0};
  unsigned edges_{0};
  bool ready_{false};
  bool sck_{true};
};

class ChipPin final : public GPIOPin {
 public:
  ChipPin(FakeChip *chip, bool is_sck) : chip_(chip), is_sck_(is_sck) {}
  void setup() override {}
  void pin_mode(gpio::Flags flags) override {}
  gpio::Flags get_flags() const override { return gpio::FLAG_NONE; }
  bool digital_read() override { return this->chip_->dout(); }
  void digital_write(bool value) override {
    if (this->is_sck_)
      this->chip_->sck(value);
  }

 protected:
  FakeChip *chip_;
  bool is_sck_;
};

class HX711Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->set_time_(1000);
    this->hx_.set_dout_pin(&this->dout_);
    this->hx_.set_sck_pin(&this->sck_);
    this->hx_.set_settling_time(SETTLING_MS);
    this->hx_.set_measurement_ready_timeout(TIMEOUT_MS);
    this->hx_.add_on_state_callback([this](float value) { this->a_values_.push_back(value); });
    this->channel_b_.add_on_state_callback([this](float value) { this->b_values_.push_back(value); });
  }

  void set_time_(uint32_t now) {
    this->now_ = now;
    LoopBlockingGuard guard(nullptr, nullptr, now);
  }
  void tick_(uint32_t ms = 16) {
    this->set_time_(this->now_ + ms);
    this->hx_.loop();
  }
  // setup(), the first conversion and settling, ending idle with the loop disabled.
  void start_(unsigned gain_pulses = 1) {
    this->hx_.setup();
    this->tick_();
    this->chip_.convert(0);
    this->tick_();
    EXPECT_EQ(this->chip_.gain_pulses(), gain_pulses);
    this->settle_();
    this->tick_();
    ASSERT_TRUE(this->hx_.is_idle());
  }
  void settle_() { this->tick_(SETTLING_MS); }
  // update(), then one conversion with the given value.
  void read_(int32_t value) {
    this->hx_.update();
    this->tick_();
    this->tick_();
    this->chip_.convert(value);
    this->tick_();
  }

  FakeChip chip_;
  ChipPin dout_{&this->chip_, false};
  ChipPin sck_{&this->chip_, true};
  HX711Sensor hx_;
  sensor::Sensor channel_b_;
  std::vector<float> a_values_;
  std::vector<float> b_values_;
  uint32_t now_{0};
};

TEST_F(HX711Test, UpdatePublishesAndDisablesTheLoop) {
  this->start_();
  this->read_(1234);
  ASSERT_EQ(this->a_values_.size(), 1u);
  EXPECT_EQ(this->a_values_[0], 1234.0f);
  EXPECT_EQ(this->chip_.gain_pulses(), 1u);
  this->tick_();
  EXPECT_TRUE(this->hx_.is_idle());
  EXPECT_FALSE(this->chip_.powered_down());
}

TEST_F(HX711Test, PowerDownAfterReadingPowersBackUpForTheNextUpdate) {
  this->hx_.set_power_down_after_reading(true);
  this->start_();
  this->read_(-5);
  ASSERT_EQ(this->a_values_.size(), 1u);
  EXPECT_EQ(this->a_values_[0], -5.0f);
  EXPECT_TRUE(this->chip_.powered_down());
  EXPECT_TRUE(this->hx_.is_idle());

  this->hx_.update();
  EXPECT_FALSE(this->chip_.powered_down());
  this->tick_();
  this->chip_.convert(0);
  this->tick_();
  this->settle_();
  this->tick_();
  this->tick_();
  this->chip_.convert(77);
  this->tick_();
  ASSERT_EQ(this->a_values_.size(), 2u);
  EXPECT_EQ(this->a_values_[1], 77.0f);
}

TEST_F(HX711Test, ChannelBIsReadAfterChannelA) {
  this->hx_.set_channel_b_sensor(&this->channel_b_);
  this->start_();
  this->read_(100);
  ASSERT_EQ(this->a_values_.size(), 1u);
  EXPECT_EQ(this->a_values_[0], 100.0f);
  EXPECT_EQ(this->chip_.gain_pulses(), 2u);
  EXPECT_TRUE(this->b_values_.empty());

  this->settle_();
  this->tick_();
  this->chip_.convert(-8388608);
  this->tick_();
  ASSERT_EQ(this->b_values_.size(), 1u);
  EXPECT_EQ(this->b_values_[0], -8388608.0f);
  EXPECT_EQ(this->chip_.gain_pulses(), 1u);
  EXPECT_EQ(this->a_values_.size(), 1u);

  this->settle_();
  this->tick_();
  EXPECT_TRUE(this->hx_.is_idle());
}

TEST_F(HX711Test, ReadyTimeoutWarnsAndTheNextUpdateRecovers) {
  this->hx_.set_channel_b_sensor(&this->channel_b_);
  this->start_();
  this->hx_.update();
  this->tick_();
  this->tick_(TIMEOUT_MS);
  EXPECT_TRUE(this->a_values_.empty());
  this->tick_();
  ASSERT_EQ(this->a_values_.size(), 1u);
  EXPECT_TRUE(std::isnan(this->a_values_[0]));
  ASSERT_EQ(this->b_values_.size(), 1u);
  EXPECT_TRUE(std::isnan(this->b_values_[0]));
  EXPECT_TRUE(this->hx_.status_has_warning());
  EXPECT_TRUE(this->chip_.powered_down());
  EXPECT_TRUE(this->hx_.is_idle());

  this->hx_.update();
  EXPECT_FALSE(this->chip_.powered_down());
  this->tick_();
  this->chip_.convert(0);
  this->tick_();
  this->settle_();
  EXPECT_FALSE(this->hx_.status_has_warning());
  this->tick_();
  this->chip_.convert(42);
  this->tick_();
  ASSERT_EQ(this->a_values_.size(), 2u);
  EXPECT_EQ(this->a_values_[1], 42.0f);
}

TEST_F(HX711Test, GainChangeDiscardsOneConversionAndSettles) {
  this->start_();
  this->hx_.set_gain(HX711_GAIN_64);
  this->read_(11);
  EXPECT_TRUE(this->a_values_.empty());
  EXPECT_EQ(this->chip_.gain_pulses(), 3u);

  this->settle_();
  this->tick_();
  this->chip_.convert(22);
  this->tick_();
  ASSERT_EQ(this->a_values_.size(), 1u);
  EXPECT_EQ(this->a_values_[0], 22.0f);
  EXPECT_EQ(this->chip_.gain_pulses(), 3u);
}

TEST_F(HX711Test, UpdateIsSkippedWhilePoweredDown) {
  this->start_();
  this->hx_.power_down();
  EXPECT_TRUE(this->chip_.powered_down());
  this->hx_.update();
  EXPECT_TRUE(this->hx_.is_idle());

  this->hx_.power_up();
  EXPECT_FALSE(this->chip_.powered_down());
  this->tick_();
  this->chip_.convert(0);
  this->tick_();
  this->settle_();
  this->tick_();
  EXPECT_TRUE(this->hx_.is_idle());
  this->read_(5);
  ASSERT_EQ(this->a_values_.size(), 1u);
  EXPECT_EQ(this->a_values_[0], 5.0f);
}

TEST_F(HX711Test, Gain32WithoutChannelBSensorCompletesTheUpdate) {
  this->hx_.set_gain(HX711_GAIN_32);
  this->start_(2);
  this->read_(9);
  ASSERT_EQ(this->a_values_.size(), 1u);
  EXPECT_EQ(this->a_values_[0], 9.0f);
  EXPECT_EQ(this->chip_.gain_pulses(), 2u);
  this->tick_();
  EXPECT_TRUE(this->hx_.is_idle());
}

TEST_F(HX711Test, Gain32MirrorsTheReadingToChannelB) {
  this->hx_.set_channel_b_sensor(&this->channel_b_);
  this->hx_.set_gain(HX711_GAIN_32);
  this->start_(2);
  this->read_(9);
  ASSERT_EQ(this->a_values_.size(), 1u);
  ASSERT_EQ(this->b_values_.size(), 1u);
  EXPECT_EQ(this->b_values_[0], 9.0f);
  this->tick_();
  EXPECT_TRUE(this->hx_.is_idle());
}

TEST_F(HX711Test, PowerDownFromAnOnValueCallbackIsKept) {
  this->hx_.add_on_state_callback([this](float) { this->hx_.power_down(); });
  this->start_();
  this->read_(3);
  ASSERT_EQ(this->a_values_.size(), 1u);
  EXPECT_TRUE(this->chip_.powered_down());
  this->tick_();
  this->hx_.update();
  this->tick_();
  this->chip_.convert(4);
  this->tick_(TIMEOUT_MS);
  EXPECT_EQ(this->a_values_.size(), 1u);
  EXPECT_TRUE(this->chip_.powered_down());
}

}  // namespace esphome::hx711::testing

#endif  // USE_HOST
