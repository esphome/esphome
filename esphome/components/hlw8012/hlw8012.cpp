#include "hlw8012.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hlw8012 {

static const char *TAG = "hlw8012";

static const uint32_t HLW8012_CLOCK_FREQUENCY = 3579000;
static const float HLW8012_REFERENCE_VOLTAGE = 2.43f;

void HLW8012Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HLW8012...");
  this->sel_pin_->setup();
  this->sel_pin_->digital_write(this->current_mode_);
  this->cf_store_.pulse_counter_setup(this->cf_pin_);
  this->cf1_store_.pulse_counter_setup(this->cf1_pin_);
}
void HLW8012Component::dump_config() {
  ESP_LOGCONFIG(TAG, "HLW8012:");
  LOG_PIN("  SEL Pin: ", this->sel_pin_)
  LOG_PIN("  CF Pin: ", this->cf_pin_)
  LOG_PIN("  CF1 Pin: ", this->cf1_pin_)
  ESP_LOGCONFIG(TAG, "  Change measurement mode every %u", this->change_mode_every_);
  ESP_LOGCONFIG(TAG, "  Current resistor: %.1f mΩ", this->current_resistor_ * 1000.0f);
  ESP_LOGCONFIG(TAG, "  Voltage Divider: %.1f", this->voltage_divider_);
  LOG_UPDATE_INTERVAL(this)
  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_)
  LOG_SENSOR("  ", "Current", this->current_sensor_)
  LOG_SENSOR("  ", "Power", this->power_sensor_)
}
float HLW8012Component::get_setup_priority() const { return setup_priority::DATA; }
void HLW8012Component::update() {
  // HLW8012 has 50% duty cycle
  pulse_counter::pulse_counter_t raw_cf = this->cf_store_.read_raw_value();
  pulse_counter::pulse_counter_t raw_cf1 = this->cf1_store_.read_raw_value();
  float cf_hz = raw_cf / (this->get_update_interval() / 1000.0f);
  if (raw_cf <= 1) {
    // don't count single pulse as power
    cf_hz = 0.0f;
  }
  float cf1_hz = raw_cf1 / (this->get_update_interval() / 1000.0f);
  if (raw_cf1 <= 1) {
    // don't count single pulse as anything
    cf1_hz = 0.0f;
  }

  if (this->nth_value_++ < 2) {
    return;
  }

  const float v_ref_squared = HLW8012_REFERENCE_VOLTAGE * HLW8012_REFERENCE_VOLTAGE;
  const float power_multiplier_micros =
      64000000.0f * v_ref_squared * this->voltage_divider_ / this->current_resistor_ / 24.0f / HLW8012_CLOCK_FREQUENCY;
  float power = cf_hz * power_multiplier_micros / 1000000.0f;

  if (this->change_mode_at_ != 0) {
    // Only read cf1 after one cycle. Apparently it's quite unstable after being changed.
    if (this->current_mode_) {
      const float current_multiplier_micros =
          512000000.0f * HLW8012_REFERENCE_VOLTAGE / this->current_resistor_ / 24.0f / HLW8012_CLOCK_FREQUENCY;
      float current = cf1_hz * current_multiplier_micros / 1000000.0f;
      ESP_LOGD(TAG, "Got power=%.1fW, current=%.1fA", power, current);
      if (this->current_sensor_ != nullptr) {
        this->current_sensor_->publish_state(current);
      }
    } else {
      const float voltage_multiplier_micros =
          256000000.0f * HLW8012_REFERENCE_VOLTAGE * this->voltage_divider_ / HLW8012_CLOCK_FREQUENCY;
      float voltage = cf1_hz * voltage_multiplier_micros / 1000000.0f;
      ESP_LOGD(TAG, "Got power=%.1fW, voltage=%.1fV", power, voltage);
      if (this->voltage_sensor_ != nullptr) {
        this->voltage_sensor_->publish_state(voltage);
      }
    }
  }

  if (this->power_sensor_ != nullptr) {
    this->power_sensor_->publish_state(power);
  }

  if (this->change_mode_at_++ == this->change_mode_every_) {
    this->current_mode_ = !this->current_mode_;
    ESP_LOGV(TAG, "Changing mode to %s mode", this->current_mode_ ? "CURRENT" : "VOLTAGE");
    this->change_mode_at_ = 0;
    this->sel_pin_->digital_write(this->current_mode_);
  }
}

}  // namespace hlw8012
}  // namespace esphome
