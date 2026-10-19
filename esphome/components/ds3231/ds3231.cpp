#include "ds3231.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

// Datasheet:
// - https://www.analog.com/media/en/technical-documentation/data-sheets/DS3231.pdf

namespace esphome::ds3231 {

ESPHOME_LOG_TAG(TAG, "ds3231");

static constexpr uint8_t DS3231_REG_CONTROL = 0x0E;
static constexpr uint8_t DS3231_REG_TEMPERATURE = 0x11;

void DS3231Component::setup() {
  if (!this->read_control_status_()) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  this->control_reg_ &= ~CONTROL_EOSC;  // keep the oscillator running on battery
  // INT/SQW is the (unused) interrupt line rather than a square-wave output.
  this->control_reg_ |= CONTROL_INTCN;
  if (!this->write_control_()) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
}

void DS3231Component::dump_config() {
  ESP_LOGCONFIG(TAG, "DS3231:");
  LOG_I2C_DEVICE(this);
}

bool DS3231Component::read_temperature(float &out) {
  uint8_t raw[2];
  if (this->is_failed() || !this->read_bytes(DS3231_REG_TEMPERATURE, raw, sizeof(raw))) {
    return false;
  }
  // 10-bit two's complement, 0.25 degC per LSB.
  int16_t value = encode_value<int16_t>(raw) >> 6;
  out = value * 0.25f;
  return true;
}

void DS3231Component::force_temperature_conversion() {
  // Just kick off the conversion - the chip clears CONV and updates the temperature
  // register on its own within ~200 ms, and the next temperature poll picks it up.
  // Blocking here to wait for it would stall the main loop.
  if (this->is_failed() || !this->read_control_status_())
    return;
  if ((this->status_reg_ & STATUS_BSY) != 0) {
    ESP_LOGW(TAG, "A temperature conversion is already in progress.");
    return;
  }
  this->control_reg_ |= CONTROL_CONV;
  this->write_control_();
}

bool DS3231Component::read_control_status_() {
  uint8_t raw[2];
  if (!this->read_bytes(DS3231_REG_CONTROL, raw, sizeof(raw))) {
    ESP_LOGW(TAG, "Can't read control/status registers.");
    return false;
  }
  this->control_reg_ = raw[0];
  this->status_reg_ = raw[1];
  return true;
}

bool DS3231Component::write_control_() {
  if (!this->write_byte(DS3231_REG_CONTROL, this->control_reg_)) {
    ESP_LOGW(TAG, "Can't write control register.");
    return false;
  }
  return true;
}

}  // namespace esphome::ds3231
