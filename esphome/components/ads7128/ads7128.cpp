#include "ads7128.h"

#include <algorithm>

#include "esphome/core/datatypes.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ads7128 {

namespace {

constexpr auto TAG = "ads7128";

// REgister Access opcodes
namespace opcode {
constexpr uint8_t WRITE = 0x08;
constexpr uint8_t READ = 0x10;
constexpr uint8_t SET_BITS = 0x18;
constexpr uint8_t CLEAR_BITS = 0x20;
constexpr uint8_t BLOCK_WRITE = 0x28;
constexpr uint8_t BLOCK_READ = 0x30;
}  // namespace opcode

}  // namespace

void ADS7128Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADS7128...");
  if (!SYSTEM_STATUS_RSVD(*this)) {
    ESP_LOGE(TAG, "ADS7128 not available under 0x%02X", this->address_);
    this->mark_failed();
    return;
  }
  this->reset_device_();
}

void ADS7128Component::digital_setup(uint8_t pin, gpio::Flags flags) {
  uint8_t mask = 0x1 << pin;
  this->set_register_bits_(Register::PIN_CFG, mask);
  if (flags & gpio::Flags::FLAG_INPUT) {
    this->any_gpi_ = true;
    this->clear_register_bits_(Register::GPIO_CFG, mask);
  } else {
    this->set_register_bits_(Register::GPIO_CFG, mask);
  }
  if (flags & gpio::Flags::FLAG_OPEN_DRAIN) {
    this->clear_register_bits_(Register::GPO_DRIVE_CFG, mask);
  } else {
    this->set_register_bits_(Register::GPO_DRIVE_CFG, mask);
  }
}

void ADS7128Component::dump_config() {
  ESP_LOGCONFIG(TAG, "ADS7128:");
  LOG_I2C_DEVICE(this)
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with ADS7128 failed!");
  }
}

void ADS7128Component::loop() {
  if (this->any_gpi_)
    this->gpi_value_ = this->read_register_(Register::GPI_VALUE);

  if (!this->sensors_.empty()) {
    bool advance = this->read_sensor_(this->sensors_.front());
    if (advance)
      this->sensors_.pop_front();
  }
}

void ADS7128Component::digital_write(uint8_t pin, bool value) {
  uint8_t mask = 0x1 << pin;
  if (value) {
    this->set_register_bits_(Register::GPO_VALUE, mask);
  } else {
    this->clear_register_bits_(Register::GPO_VALUE, mask);
  }
}

void ADS7128Component::sensor_update(ADS7128Sensor *sensor) {
  if (std::find(this->sensors_.begin(), this->sensors_.end(), sensor) != this->sensors_.end())
    return;  // Already queued
  this->sensors_.push_back(sensor);
}

uint8_t ADS7128Component::read_register_(Register r) {
  uint8_t command[2] = {opcode::READ, uint8_t(r)};
  if (this->write(command, 2) != i2c::ERROR_OK) {
    this->status_set_warning();
    return 0x00;
  }
  uint8_t value;
  if (this->read(&value, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return 0x00;
  }

  return value;
}

void ADS7128Component::write_register_(Register r, uint8_t value) {
  uint8_t command[3] = {opcode::WRITE, uint8_t(r), value};
  if (this->write(command, 3) != i2c::ERROR_OK) {
    this->status_set_warning();
  }
}

void ADS7128Component::set_register_bits_(Register r, uint8_t mask) {
  uint8_t command[3] = {opcode::SET_BITS, uint8_t(r), mask};
  if (this->write(command, 3) != i2c::ERROR_OK) {
    this->status_set_warning();
  }
}

void ADS7128Component::clear_register_bits_(Register r, uint8_t mask) {
  uint8_t command[3] = {opcode::CLEAR_BITS, uint8_t(r), mask};
  if (this->write(command, 3) != i2c::ERROR_OK) {
    this->status_set_warning();
  }
}

bool ADS7128Component::read_sensor_(ADS7128Sensor *sensor) {
  // In addition to the datasheet, see also
  // https://e2e.ti.com/support/data-converters-group/data-converters/f/data-converters-forum/1225819/faq-ads7128-using-the-rms-module
  if (!this->is_waiting_) {
    OSR(*this) = sensor->get_oversampling();
    OSC_SEL(*this) = sensor->get_osc_sel();
    CLK_DIV(*this) = sensor->get_clk_div();
    if (sensor->get_rms()) {
      RMS_CHID(*this) = sensor->get_channel();
      RMS_DC_SUB(*this) = true;
      RMS_SAMPLES(*this) = sensor->get_rms_samples();
      RMS_EN(*this) = true;
      write_register_(Register::AUTO_SEQ_CH_SEL, 1 << sensor->get_channel());
      SEQ_MODE(*this) = 1;
      CONV_MODE(*this) = 1;
      SEQ_START(*this) = true;
      this->wait_time_ms_ = millis() + sensor->get_rms_read_time_us() / 1000;
      this->is_waiting_ = true;
      return false;
    } else {
      SEQ_MODE(*this) = 0;
      MANUAL_CHID(*this) = sensor->get_channel();
    }
  }
  if (this->is_waiting_ && int32_t(millis() - this->wait_time_ms_) < 0) {
    return false;  // Chip is still sampling; don't bother checking yet
  }
  if (sensor->get_rms()) {
    if (RMS_DONE(*this)) {
      SEQ_START(*this) = false;
      auto value = uint16_t(read_register_(Register::RMS_MSB)) << 8 | read_register_(Register::RMS_LSB);
      sensor->publish_state(float(value) / 0xFFFF);
      RMS_DONE(*this).set();
      RMS_EN(*this) = false;
      CONV_MODE(*this) = 0;
      this->is_waiting_ = false;
      return true;
    }
    return false;  // Keep waiting
  } else {
    uint16_be_t value;
    auto result = read(reinterpret_cast<uint8_t *>(&value), 2);
    if (result == i2c::ERROR_OK) {
      sensor->publish_state(float(value) / 0xFFFF);
    } else {
      this->status_set_warning();
    }
    this->is_waiting_ = false;
    return true;
  }
}

std::string ADS7128GPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%u via ADS7128", pin_);
  return buffer;
}

void ADS7128Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "  Analog Sensor Channel %d", this->get_channel());
  ESP_LOGCONFIG(TAG, "    Cycle Time %dns", this->get_cycle_time_ns());
  ESP_LOGCONFIG(TAG, "    Oversampling %d (%dns)", this->get_oversampling_count(), get_sample_time_ns());
  if (this->rms_) {
    ESP_LOGCONFIG(TAG, "    RMS enabled with %d samples (%dus)", this->get_rms_samples_count(), this->get_rms_us());
  }
}

}  // namespace ads7128
}  // namespace esphome
