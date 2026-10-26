#include "sy6970.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::sy6970 {

ESPHOME_LOG_TAG(TAG, "sy6970");

static const LogString *i2c_watchdog_timeout_to_string(I2CWatchdogTimeout timeout) {
  switch (timeout) {
    case I2C_WATCHDOG_DISABLED:
      return LOG_STR("Disabled");
    case I2C_WATCHDOG_40S:
      return LOG_STR("40s");
    case I2C_WATCHDOG_80S:
      return LOG_STR("80s");
    case I2C_WATCHDOG_160S:
      return LOG_STR("160s");
    default:
      return LOG_STR("Unknown");
  }
}

static constexpr uint32_t I2C_WATCHDOG_KICK_INTERVAL_ID = 0;
// Well under the 40 s default, so a failed kick or a timeout the chip did not accept still leaves margin
static constexpr uint32_t I2C_WATCHDOG_KICK_INTERVAL_MS = 10000;

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
  if (this->update_register(reg, mask, value))
    return true;
  ESP_LOGW(TAG, "Failed to update register 0x%02X", reg);
  return false;
}

void SY6970Component::setup() {
  ESP_LOGV(TAG, "Setting up SY6970...");

  // Try to read chip ID
  uint8_t reg_value;
  if (!this->read_byte(SY6970_REG_DEVICE_ID, &reg_value)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  uint8_t chip_id = reg_value & 0x03;
  if (chip_id != 0x00) {
    ESP_LOGW(TAG, "Unexpected chip ID: 0x%02X (expected 0x00)", chip_id);
  }

  // Service the watchdog first, so the settings below are not reverted while they are written
  ESP_LOGV(TAG, "Setting I2C watchdog timeout to %s",
           LOG_STR_ARG(i2c_watchdog_timeout_to_string(this->i2c_watchdog_timeout_)));
  this->set_i2c_watchdog_timeout(this->i2c_watchdog_timeout_);

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
                "  ADC Enabled: %s\n"
                "  I2C Watchdog Timeout: %s",
                ONOFF(this->led_enabled_), this->input_current_limit_, this->charge_voltage_, this->charge_current_,
                this->precharge_current_, ONOFF(this->charge_enabled_), ONOFF(this->enable_adc_),
                LOG_STR_ARG(i2c_watchdog_timeout_to_string(this->i2c_watchdog_timeout_)));
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
}

void SY6970Component::update() {
  if (this->is_failed()) {
    return;
  }

  // Read all registers in one transaction
  if (!this->status_comm_result(this->read_all_registers_())) {
    ESP_LOGW(TAG, "Failed to read registers during update");
    return;
  }

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

void SY6970Component::set_i2c_watchdog_timeout(I2CWatchdogTimeout timeout) {
  if (this->is_failed())
    return;

  if (timeout != I2C_WATCHDOG_DISABLED) {
    // Changing the timeout does not restart the chip's timer, which may be close to expiring
    this->reset_i2c_watchdog();
  }

  // REG07 bits 5:4 (WATCHDOG[1:0])
  bool written = this->update_register_(SY6970_REG_TIMER_CONTROL, 0x30, static_cast<uint8_t>(timeout) << 4);
  if (!written) {
    ESP_LOGW(TAG, "Failed to set I2C watchdog timeout to %s", LOG_STR_ARG(i2c_watchdog_timeout_to_string(timeout)));
  }

  if (timeout == I2C_WATCHDOG_DISABLED && written) {
    this->i2c_watchdog_timeout_ = timeout;
    this->cancel_interval(I2C_WATCHDOG_KICK_INTERVAL_ID);
    return;
  }
  if (timeout != I2C_WATCHDOG_DISABLED) {
    this->i2c_watchdog_timeout_ = timeout;
  }
  // Keeps kicking after a failed write too: the chip may still be on its 40 s default
  this->set_interval(I2C_WATCHDOG_KICK_INTERVAL_ID, I2C_WATCHDOG_KICK_INTERVAL_MS,
                     [this]() { this->reset_i2c_watchdog(); });
}

bool SY6970Component::reset_i2c_watchdog() {
  if (this->is_failed())
    return false;

  // WD_RST, self-clearing
  if (!this->update_register_(SY6970_REG_SYS_CONTROL, 0x40, 0x40)) {
    ESP_LOGW(TAG, "I2C watchdog reset failed, settings may revert");
    return false;
  }
  return true;
}

}  // namespace esphome::sy6970
