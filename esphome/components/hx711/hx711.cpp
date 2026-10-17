#include "hx711.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::hx711 {

ESPHOME_LOG_TAG(TAG, "hx711");

static constexpr unsigned gain_value(HX711Gain gain) {
  switch (gain) {
    case HX711_GAIN_32:
      return 32;
    case HX711_GAIN_64:
      return 64;
    default:
      return 128;
  }
}

void HX711Sensor::setup() {
  this->sck_pin_->setup();
  this->dout_pin_->setup();
  this->power_on_();
}

void HX711Sensor::dump_config() {
  LOG_SENSOR("", "HX711", this);
  LOG_PIN("  DOUT Pin: ", this->dout_pin_);
  LOG_PIN("  SCK Pin: ", this->sck_pin_);
  ESP_LOGCONFIG(TAG,
                "  Gain: x%u\n"
                "  Settling time: %ums\n"
                "  Measurement ready timeout: %ums\n"
                "  Power down after reading: %s",
                gain_value(this->gain_), this->settling_time_ms_, this->measurement_ready_timeout_ms_,
                YESNO(this->power_down_after_reading_));
#ifdef USE_HX711_CHANNEL_B_SENSOR
  LOG_SENSOR("  ", "Channel B", this->channel_b_sensor_);
#endif
  LOG_UPDATE_INTERVAL(this);
}

void HX711Sensor::on_powerdown() { this->sck_pin_->digital_write(true); }

void HX711Sensor::update() {
  if (this->phase_ == Phase::SLEEPING) {
    ESP_LOGV(TAG, "Powered down, skipping update");
    return;
  }
  if (this->phase_ == Phase::READING) {
    ESP_LOGW(TAG, "Previous update still in progress");
    return;
  }
  this->update_pending_ = true;
  if (this->phase_ == Phase::POWERED_DOWN)
    this->power_on_();
  this->enable_loop();
}

void HX711Sensor::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  switch (this->phase_) {
    case Phase::WAKING: {
      if (this->dout_pin_->digital_read()) {
        if (this->timed_out_(now))
          this->fail_update_(LOG_STR("Not ready after power-up"));
        return;
      }
      // Discard the first conversion; the pulses after it apply the configured gain.
      int32_t unused;
      if (!this->read_raw_(unused, this->gain_)) {
        this->fail_update_(LOG_STR("DOUT not high after read"));
        return;
      }
      this->phase_ = Phase::SETTLING;
      this->phase_start_ = now;
      return;
    }
    case Phase::SETTLING:
      if (now - this->phase_start_ >= this->settling_time_ms_) {
        this->phase_ = Phase::IDLE;
        this->status_clear_warning();
      }
      return;
    case Phase::IDLE:
      if (!this->update_pending_) {
        this->disable_loop();
        return;
      }
      this->phase_ = Phase::READING;
      this->phase_start_ = now;
      return;
    case Phase::READING:
      break;
    case Phase::SLEEPING:
    case Phase::POWERED_DOWN:
      this->disable_loop();
      return;
  }

  if (this->dout_pin_->digital_read()) {
    if (this->timed_out_(now))
      this->fail_update_(LOG_STR("Measurement not ready"));
    return;
  }

  const HX711Gain conversion_gain = this->last_gain_;
#ifdef USE_HX711_CHANNEL_B_SENSOR
  const bool channel_b = this->channel_b_pending_;
#else
  constexpr bool channel_b = false;
#endif
  // After set_gain() the waiting conversion still used the old gain: discard it and settle.
  const bool discard = !channel_b && conversion_gain != this->gain_;
  HX711Gain next_gain = this->gain_;
#ifdef USE_HX711_CHANNEL_B_SENSOR
  // A channel A reading is followed by one on channel B; with gain 32 the main sensor already reads channel B
  const bool schedule_b = !channel_b && !discard && this->channel_b_sensor_ != nullptr && this->gain_ != HX711_GAIN_32;
  if (schedule_b)
    next_gain = HX711_GAIN_32;
#else
  constexpr bool schedule_b = false;
#endif

  int32_t value;
  if (!this->read_raw_(value, next_gain)) {
    this->fail_update_(LOG_STR("DOUT not high after read"));
    return;
  }

  sensor::Sensor *target = nullptr;
  if (!discard) {
    target = this;
#ifdef USE_HX711_CHANNEL_B_SENSOR
    if (channel_b)
      target = this->channel_b_sensor_;
    this->channel_b_pending_ = schedule_b;
#endif
  }
  // Finish the phase change before publishing: an on_value automation may call power_down()
  const bool done = !discard && !schedule_b;
  if (done)
    this->update_pending_ = false;
  if (done && this->power_down_after_reading_) {
    this->power_off_(Phase::POWERED_DOWN);
  } else if (next_gain != conversion_gain) {
    this->phase_ = Phase::SETTLING;
    this->phase_start_ = now;
  } else {
    this->phase_ = Phase::IDLE;
  }
  if (target == nullptr)
    return;
  ESP_LOGD(TAG, "'%s': Got value %" PRId32 " (gain x%u)", target->get_name().c_str(), value,
           gain_value(conversion_gain));
  target->publish_state(value);
#ifdef USE_HX711_CHANNEL_B_SENSOR
  if (!channel_b && conversion_gain == HX711_GAIN_32 && this->channel_b_sensor_ != nullptr)
    this->channel_b_sensor_->publish_state(value);
#endif
}

void HX711Sensor::power_up() {
  if (this->phase_ != Phase::SLEEPING)
    return;
  ESP_LOGV(TAG, "Powering up");
  this->power_on_();
  this->enable_loop();
}

void HX711Sensor::power_down() {
  if (this->phase_ == Phase::SLEEPING)
    return;
  ESP_LOGV(TAG, "Powering down");
  this->power_off_(Phase::SLEEPING);
}

void HX711Sensor::power_on_() {
  // PD_SCK must stay high for 60us to power the chip down; cover a power_down() followed at once by power_up()
  delayMicroseconds(60);
  // Power-up resets the chip to channel A, gain 128.
  this->sck_pin_->digital_write(false);
  this->last_gain_ = HX711_GAIN_128;
  this->phase_ = Phase::WAKING;
  this->phase_start_ = App.get_loop_component_start_time();
}

void HX711Sensor::power_off_(Phase phase) {
  this->sck_pin_->digital_write(true);
  this->update_pending_ = false;
#ifdef USE_HX711_CHANNEL_B_SENSOR
  this->channel_b_pending_ = false;
#endif
  this->phase_ = phase;
  this->disable_loop();
}

void HX711Sensor::fail_update_(const LogString *reason) {
  // Note what is still owed and finish the power change first: an on_value automation may call power_down()
  bool owe_a = this->update_pending_;
#ifdef USE_HX711_CHANNEL_B_SENSOR
  owe_a = owe_a && !this->channel_b_pending_;
  const bool owe_b = this->update_pending_ && this->channel_b_sensor_ != nullptr;
#endif
  this->status_set_warning(reason);
  this->power_off_(Phase::POWERED_DOWN);
  if (owe_a)
    this->publish_state(NAN);
#ifdef USE_HX711_CHANNEL_B_SENSOR
  if (owe_b)
    this->channel_b_sensor_->publish_state(NAN);
#endif
}

bool HX711Sensor::read_raw_(int32_t &value, HX711Gain next_gain) {
  uint32_t data = 0;
  bool final_dout;
  {
    InterruptLock lock;
    for (uint8_t i = 0; i < 24; i++) {
      this->sck_pin_->digital_write(true);
      delayMicroseconds(1);
      data |= uint32_t(this->dout_pin_->digital_read()) << (23 - i);
      this->sck_pin_->digital_write(false);
      delayMicroseconds(1);
    }
    for (uint8_t i = 0; i < next_gain; i++) {
      this->sck_pin_->digital_write(true);
      delayMicroseconds(1);
      this->sck_pin_->digital_write(false);
      delayMicroseconds(1);
    }
    final_dout = this->dout_pin_->digital_read();
  }
  this->last_gain_ = next_gain;
  value = sign_extend<24>(data);
  return final_dout;
}

}  // namespace esphome::hx711
