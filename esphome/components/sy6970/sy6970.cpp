#include "sy6970.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::sy6970 {

static const char *const TAG = "sy6970";

// Constants for voltage and current calculations
static const uint16_t VBUS_BASE = 2600;         // mV
static const uint16_t VBUS_STEP = 100;          // mV
static const uint16_t VBAT_BASE = 2304;         // mV
static const uint16_t VBAT_STEP = 20;           // mV
static const uint16_t VSYS_BASE = 2304;         // mV
static const uint16_t VSYS_STEP = 20;           // mV
static const uint16_t CHG_CURRENT_STEP = 50;    // mA
static const uint16_t PRE_CHG_BASE = 64;        // mA
static const uint16_t PRE_CHG_STEP = 64;        // mA
static const uint16_t CHG_VOLTAGE_BASE = 3840;  // mV
static const uint16_t CHG_VOLTAGE_STEP = 16;    // mV
static const uint16_t INPUT_CURRENT_MIN = 100;  // mA
static const uint16_t INPUT_CURRENT_STEP = 50;  // mA

bool SY6970Component::read_all_registers_() {
  // Read all registers from 0x00 to 0x14 in one transaction (21 bytes)
  // This includes unused registers 0x0F, 0x10, 0x13 for performance
  if (!this->read_bytes(SY6970_REG_INPUT_CURRENT_LIMIT, this->data_.registers, 21)) {
    ESP_LOGW(TAG, "Failed to read registers 0x00-0x14");
    return false;
  }

  return true;
}

bool SY6970Component::write_register_(uint8_t reg, uint8_t value) {
  if (!this->write_byte(reg, value)) {
    ESP_LOGW(TAG, "Failed to write register 0x%02X", reg);
    return false;
  }
  return true;
}

bool SY6970Component::update_register_(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t reg_value;
  if (!this->read_byte(reg, &reg_value)) {
    ESP_LOGW(TAG, "Failed to read register 0x%02X for update", reg);
    return false;
  }
  reg_value = (reg_value & ~mask) | (value & mask);
  return this->write_register_(reg, reg_value);
}

void SY6970Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SY6970...");

  // Try to read chip ID
  uint8_t reg_value;
  if (!this->read_byte(SY6970_REG_DEVICE_ID, &reg_value)) {
    ESP_LOGE(TAG, "Failed to communicate with SY6970");
    this->mark_failed();
    return;
  }

  uint8_t chip_id = reg_value & 0x03;
  if (chip_id != 0x00) {
    ESP_LOGW(TAG, "Unexpected chip ID: 0x%02X (expected 0x00)", chip_id);
  }

  ESP_LOGCONFIG(TAG, "SY6970 initialized successfully");
}

void SY6970Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SY6970:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with SY6970 failed!");
  }
  ESP_LOGCONFIG(TAG, "  Listeners: %d", this->listeners_.size());
}

void SY6970Component::update() {
  if (this->is_failed()) {
    return;
  }

  // Read all registers in one transaction
  if (!this->read_all_registers_()) {
    ESP_LOGW(TAG, "Failed to read registers during update");
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();

  // Notify all listeners with the new data
  for (auto *listener : this->listeners_) {
    listener->on_data(this->data_);
  }
}

void SY6970Component::set_input_current_limit(uint16_t milliamps) {
  if (this->is_failed())
    return;

  if (milliamps < INPUT_CURRENT_MIN) {
    milliamps = INPUT_CURRENT_MIN;
  }

  uint8_t val = (milliamps - INPUT_CURRENT_MIN) / INPUT_CURRENT_STEP;
  if (val > 0x3F) {
    val = 0x3F;
  }

  this->update_register_(SY6970_REG_INPUT_CURRENT_LIMIT, 0x3F, val);
}

void SY6970Component::set_charge_target_voltage(uint16_t millivolts) {
  if (this->is_failed())
    return;

  if (millivolts < CHG_VOLTAGE_BASE) {
    millivolts = CHG_VOLTAGE_BASE;
  }

  uint8_t val = (millivolts - CHG_VOLTAGE_BASE) / CHG_VOLTAGE_STEP;
  if (val > 0x3F) {
    val = 0x3F;
  }

  this->update_register_(SY6970_REG_CHARGE_VOLTAGE, 0xFC, val << 2);
}

void SY6970Component::set_precharge_current(uint16_t milliamps) {
  if (this->is_failed())
    return;

  if (milliamps < PRE_CHG_BASE) {
    milliamps = PRE_CHG_BASE;
  }

  uint8_t val = (milliamps - PRE_CHG_BASE) / PRE_CHG_STEP;
  if (val > 0x0F) {
    val = 0x0F;
  }

  this->update_register_(SY6970_REG_PRECHARGE_CURRENT, 0xF0, val << 4);
}

void SY6970Component::set_charge_current(uint16_t milliamps) {
  if (this->is_failed())
    return;

  uint8_t val = milliamps / 64;
  if (val > 0x7F) {
    val = 0x7F;
  }

  this->update_register_(SY6970_REG_CHARGE_CURRENT, 0x7F, val);
}

void SY6970Component::set_charge_enabled(bool enabled) {
  if (this->is_failed())
    return;

  this->update_register_(SY6970_REG_SYS_CONTROL, 0x10, enabled ? 0x10 : 0x00);
}

void SY6970Component::set_led_enabled(bool enabled) {
  if (this->is_failed())
    return;

  // Clear bit 6 to enable LED
  this->update_register_(SY6970_REG_TIMER_CONTROL, 0x40, enabled ? 0x00 : 0x40);
}

void SY6970Component::enable_adc_measure() {
  if (this->is_failed())
    return;

  // Set bits to enable ADC conversion
  this->update_register_(SY6970_REG_ADC_CONTROL, 0xC0, 0xC0);
}

}  // namespace esphome::sy6970
