#include "sy6970.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::sy6970 {

static const char *const TAG = "sy6970";

bool SY6970Component::read_all_registers_() {
  // Read all registers from 0x00 to 0x14 in one transaction (21 bytes)
  // This includes unused registers 0x0F, 0x10 for performance
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
  ESP_LOGV(TAG, "Setting up SY6970...");

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

  // Apply configuration options (all have defaults now)
  ESP_LOGV(TAG, "Setting LED enabled to %s", ONOFF(this->led_enabled_));
  this->set_led_enabled(this->led_enabled_);

  ESP_LOGV(TAG, "Setting input current limit to %u mA", this->input_current_limit_);
  this->set_input_current_limit(this->input_current_limit_);

  ESP_LOGV(TAG, "Setting charge voltage to %u mV", this->charge_voltage_);
  this->set_charge_target_voltage(this->charge_voltage_);

  ESP_LOGV(TAG, "Setting charge current to %u mA", this->charge_current_);
  this->set_charge_current(this->charge_current_);

  ESP_LOGV(TAG, "Setting precharge current to %u mA", this->precharge_current_);
  this->set_precharge_current(this->precharge_current_);

  ESP_LOGV(TAG, "Setting charge enabled to %s", ONOFF(this->charge_enabled_));
  this->set_charge_enabled(this->charge_enabled_);

  ESP_LOGV(TAG, "Setting ADC measurements to %s", ONOFF(this->enable_adc_));
  this->set_enable_adc_measure(this->enable_adc_);

  ESP_LOGV(TAG, "SY6970 initialized successfully");
}

void SY6970Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "SY6970:\n"
                "  LED Enabled: %s\n"
                "  Input Current Limit: %u mA\n"
                "  Charge Voltage: %u mV\n"
                "  Charge Current: %u mA\n"
                "  Precharge Current: %u mA\n"
                "  Charge Enabled: %s\n"
                "  ADC Enabled: %s",
                ONOFF(this->led_enabled_), this->input_current_limit_, this->charge_voltage_, this->charge_current_,
                this->precharge_current_, ONOFF(this->charge_enabled_), ONOFF(this->enable_adc_));
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with SY6970 failed!");
  }
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

  if (milliamps < PRE_CHG_BASE_MA) {
    milliamps = PRE_CHG_BASE_MA;
  }

  uint8_t val = (milliamps - PRE_CHG_BASE_MA) / PRE_CHG_STEP_MA;
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

  // Bit 6: 0 = LED enabled, 1 = LED disabled
  this->update_register_(SY6970_REG_TIMER_CONTROL, 0x40, enabled ? 0x00 : 0x40);
}

void SY6970Component::set_enable_adc_measure(bool enabled) {
  if (this->is_failed())
    return;

  // Set bits to enable ADC conversion
  this->update_register_(SY6970_REG_ADC_CONTROL, 0xC0, enabled ? 0xC0 : 0x00);
}

}  // namespace esphome::sy6970
