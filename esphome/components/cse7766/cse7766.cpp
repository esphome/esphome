#include "cse7766.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cse7766 {

static const char *const TAG = "cse7766";

void CSE7766Component::loop() {
  const uint32_t now = millis();
  if (now - this->last_transmission_ >= 500) {
    // last transmission too long ago. Reset RX index.
    this->raw_data_index_ = 0;
  }

  if (this->available() == 0)
    return;

  this->last_transmission_ = now;
  while (this->available() != 0) {
    this->read_byte(&this->raw_data_[this->raw_data_index_]);
    if (!this->check_byte_()) {
      this->raw_data_index_ = 0;
      this->status_set_warning();
    }

    if (this->raw_data_index_ == 23) {
      this->parse_data_();
      this->status_clear_warning();
    }

    this->raw_data_index_ = (this->raw_data_index_ + 1) % 24;
  }
}
float CSE7766Component::get_setup_priority() const { return setup_priority::DATA; }

bool CSE7766Component::check_byte_() {
  uint8_t index = this->raw_data_index_;
  uint8_t byte = this->raw_data_[index];
  if (index == 0) {
    return !((byte != 0x55) && ((byte & 0xF0) != 0xF0) && (byte != 0xAA));
  }

  if (index == 1) {
    if (byte != 0x5A) {
      ESP_LOGV(TAG, "Invalid Header 2 Start: 0x%02X!", byte);
      return false;
    }
    return true;
  }

  if (index == 23) {
    uint8_t checksum = 0;
    for (uint8_t i = 2; i < 23; i++)
      checksum += this->raw_data_[i];

    if (checksum != this->raw_data_[23]) {
      ESP_LOGW(TAG, "Invalid checksum from CSE7766: 0x%02X != 0x%02X", checksum, this->raw_data_[23]);
      return false;
    }
    return true;
  }

  return true;
}
void CSE7766Component::parse_data_() {
  ESP_LOGVV(TAG, "CSE7766 Data: ");
  for (uint8_t i = 0; i < 23; i++) {
    ESP_LOGVV(TAG, "  i=%u: 0b" BYTE_TO_BINARY_PATTERN " (0x%02X)", i, BYTE_TO_BINARY(this->raw_data_[i]),
              this->raw_data_[i]);
  }

  uint8_t header1 = this->raw_data_[0];
  if (header1 == 0xAA) {
    ESP_LOGW(TAG, "CSE7766 not calibrated!");
    return;
  }

  if ((header1 & 0xF0) == 0xF0 && ((header1 >> 0) & 1) == 1) {
    ESP_LOGW(TAG, "CSE7766 reports abnormal hardware: (0x%02X)", header1);
    ESP_LOGW(TAG, "  Coefficient storage area is abnormal.");
    return;
  }

  uint32_t voltage_calib = this->get_24_bit_uint_(2);
  uint32_t voltage_cycle = this->get_24_bit_uint_(5);
  uint32_t current_calib = this->get_24_bit_uint_(8);
  uint32_t current_cycle = this->get_24_bit_uint_(11);
  uint32_t power_calib = this->get_24_bit_uint_(14);
  uint32_t power_cycle = this->get_24_bit_uint_(17);

  uint8_t adj = this->raw_data_[20];
  uint32_t cf_pulses = (this->raw_data_[21] << 8) + this->raw_data_[22];

  bool power_ok = true;
  bool voltage_ok = true;
  bool current_ok = true;

  if (header1 > 0xF0) {
    // ESP_LOGV(TAG, "CSE7766 reports abnormal hardware: (0x%02X)", byte);
    if ((header1 >> 3) & 1) {
      ESP_LOGV(TAG, "  Voltage cycle exceeds range.");
      voltage_ok = false;
    }
    if ((header1 >> 2) & 1) {
      ESP_LOGV(TAG, "  Current cycle exceeds range.");
      current_ok = false;
    }
    if ((header1 >> 1) & 1) {
      ESP_LOGV(TAG, "  Power cycle exceeds range.");
      power_ok = false;
    }
    if ((header1 >> 0) & 1) {
      ESP_LOGV(TAG, "  Coefficient storage area is abnormal.");
      return;
    }
  }

  if ((adj & 0x40) == 0x40 && voltage_ok && current_ok) {
    // voltage cycle of serial port outputted is a complete cycle;
    this->voltage_acc_ += voltage_calib / float(voltage_cycle);
    this->voltage_counts_ += 1;
  }

  float power = 0;
  if ((adj & 0x10) == 0x10 && voltage_ok && current_ok && power_ok) {
    // power cycle of serial port outputted is a complete cycle;
    power = power_calib / float(power_cycle);
    this->power_acc_ += power;
    this->power_counts_ += 1;

    uint32_t difference;
    if (this->cf_pulses_last_ == 0)
      this->cf_pulses_last_ = cf_pulses;

    if (cf_pulses < this->cf_pulses_last_) {
      difference = cf_pulses + (0x10000 - this->cf_pulses_last_);
    } else {
      difference = cf_pulses - this->cf_pulses_last_;
    }
    this->cf_pulses_last_ = cf_pulses;
    this->energy_total_ += difference * float(power_calib) / 1000000.0 / 3600.0;
  }

  if ((adj & 0x20) == 0x20 && current_ok && voltage_ok && power != 0.0) {
    // indicates current cycle of serial port outputted is a complete cycle;
    this->current_acc_ += current_calib / float(current_cycle);
    this->current_counts_ += 1;
  }
}
void CSE7766Component::update() {
  float voltage = this->voltage_counts_ > 0 ? this->voltage_acc_ / this->voltage_counts_ : 0.0f;
  float current = this->current_counts_ > 0 ? this->current_acc_ / this->current_counts_ : 0.0f;
  float power = this->power_counts_ > 0 ? this->power_acc_ / this->power_counts_ : 0.0f;

  ESP_LOGV(TAG, "Got voltage_acc=%.2f current_acc=%.2f power_acc=%.2f", this->voltage_acc_, this->current_acc_,
           this->power_acc_);
  ESP_LOGV(TAG, "Got voltage_counts=%d current_counts=%d power_counts=%d", this->voltage_counts_, this->current_counts_,
           this->power_counts_);
  ESP_LOGD(TAG, "Got voltage=%.1fV current=%.1fA power=%.1fW", voltage, current, power);

  if (this->voltage_sensor_ != nullptr)
    this->voltage_sensor_->publish_state(voltage);
  if (this->current_sensor_ != nullptr)
    this->current_sensor_->publish_state(current);
  if (this->power_sensor_ != nullptr)
    this->power_sensor_->publish_state(power);
  if (this->energy_sensor_ != nullptr)
    this->energy_sensor_->publish_state(this->energy_total_);

  this->voltage_acc_ = 0.0f;
  this->current_acc_ = 0.0f;
  this->power_acc_ = 0.0f;
  this->voltage_counts_ = 0;
  this->power_counts_ = 0;
  this->current_counts_ = 0;
}

uint32_t CSE7766Component::get_24_bit_uint_(uint8_t start_index) {
  return (uint32_t(this->raw_data_[start_index]) << 16) | (uint32_t(this->raw_data_[start_index + 1]) << 8) |
         uint32_t(this->raw_data_[start_index + 2]);
}

void CSE7766Component::dump_config() {
  ESP_LOGCONFIG(TAG, "CSE7766:");
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Current", this->current_sensor_);
  LOG_SENSOR("  ", "Power", this->power_sensor_);
  LOG_SENSOR("  ", "Energy", this->energy_sensor_);
  this->check_uart_settings(4800);
}

}  // namespace cse7766
}  // namespace esphome
