#include "sy6970.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sy6970 {

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

bool SY6970Component::read_register_(uint8_t reg, uint8_t *value) {
  if (!this->read_byte(reg, value)) {
    ESP_LOGW(TAG, "Failed to read register 0x%02X", reg);
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
  if (!this->read_register_(reg, &reg_value)) {
    return false;
  }
  reg_value = (reg_value & ~mask) | (value & mask);
  return this->write_register_(reg, reg_value);
}

void SY6970Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SY6970...");

  // Try to read chip ID
  uint8_t reg_value;
  if (!this->read_register_(SY6970_REG_14, &reg_value)) {
    ESP_LOGE(TAG, "Failed to communicate with SY6970");
    this->mark_failed();
    return;
  }

  uint8_t chip_id = reg_value & 0x03;
  if (chip_id != 0x00) {
    ESP_LOGW(TAG, "Unexpected chip ID: 0x%02X (expected 0x00)", chip_id);
  }

  this->initialized_ = true;
  ESP_LOGCONFIG(TAG, "SY6970 initialized successfully");
}

void SY6970Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SY6970:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with SY6970 failed!");
  }
}

void SY6970Component::loop() {
  // Regular updates handled by sensor components
}

uint16_t SY6970Component::get_vbus_voltage() {
  if (!this->initialized_)
    return 0;

  uint8_t reg_value;
  if (!this->read_register_(SY6970_REG_11, &reg_value)) {
    return 0;
  }

  uint8_t vbus_val = reg_value & 0x7F;
  return VBUS_BASE + (vbus_val * VBUS_STEP);
}

uint16_t SY6970Component::get_battery_voltage() {
  if (!this->initialized_)
    return 0;

  uint8_t reg_value;
  if (!this->read_register_(SY6970_REG_0E, &reg_value)) {
    return 0;
  }

  uint8_t vbat_val = reg_value & 0x7F;
  return VBAT_BASE + (vbat_val * VBAT_STEP);
}

uint16_t SY6970Component::get_system_voltage() {
  if (!this->initialized_)
    return 0;

  uint8_t reg_value;
  if (!this->read_register_(SY6970_REG_0D, &reg_value)) {
    return 0;
  }

  uint8_t vsys_val = reg_value & 0x7F;
  return VSYS_BASE + (vsys_val * VSYS_STEP);
}

uint16_t SY6970Component::get_charge_current() {
  if (!this->initialized_)
    return 0;

  uint8_t reg_value;
  if (!this->read_register_(SY6970_REG_12, &reg_value)) {
    return 0;
  }

  uint8_t ichg_val = reg_value & 0x7F;
  return ichg_val * CHG_CURRENT_STEP;
}

uint16_t SY6970Component::get_precharge_current() {
  if (!this->initialized_)
    return 0;

  uint8_t reg_value;
  if (!this->read_register_(SY6970_REG_05, &reg_value)) {
    return 0;
  }

  uint8_t iprechg = (reg_value >> 4) & 0x0F;
  return PRE_CHG_BASE + (iprechg * PRE_CHG_STEP);
}

bool SY6970Component::is_vbus_connected() {
  if (!this->initialized_)
    return false;

  uint8_t reg_value;
  if (!this->read_register_(SY6970_REG_0B, &reg_value)) {
    return false;
  }

  uint8_t bus_status = (reg_value >> 5) & 0x07;
  return bus_status != BUS_STATUS_NO_INPUT;
}

bool SY6970Component::is_charging() {
  if (!this->initialized_)
    return false;

  uint8_t reg_value;
  if (!this->read_register_(SY6970_REG_0B, &reg_value)) {
    return false;
  }

  uint8_t chrg_stat = (reg_value >> 3) & 0x03;
  return chrg_stat != CHARGE_STATUS_NOT_CHARGING && chrg_stat != CHARGE_STATUS_CHARGE_DONE;
}

bool SY6970Component::is_charge_done() {
  if (!this->initialized_)
    return false;

  uint8_t reg_value;
  if (!this->read_register_(SY6970_REG_0B, &reg_value)) {
    return false;
  }

  uint8_t chrg_stat = (reg_value >> 3) & 0x03;
  return chrg_stat == CHARGE_STATUS_CHARGE_DONE;
}

const char *SY6970Component::get_bus_status_string() {
  uint8_t status = this->get_bus_status();
  switch (status) {
    case BUS_STATUS_NO_INPUT:
      return "No Input";
    case BUS_STATUS_USB_SDP:
      return "USB SDP";
    case BUS_STATUS_USB_CDP:
      return "USB CDP";
    case BUS_STATUS_USB_DCP:
      return "USB DCP";
    case BUS_STATUS_HVDCP:
      return "HVDCP";
    case BUS_STATUS_ADAPTER:
      return "Adapter";
    case BUS_STATUS_NO_STD_ADAPTER:
      return "Non-Standard Adapter";
    case BUS_STATUS_OTG:
      return "OTG";
    default:
      return "Unknown";
  }
}

const char *SY6970Component::get_charge_status_string() {
  uint8_t status = this->get_charge_status();
  switch (status) {
    case CHARGE_STATUS_NOT_CHARGING:
      return "Not Charging";
    case CHARGE_STATUS_PRE_CHARGE:
      return "Pre-charge";
    case CHARGE_STATUS_FAST_CHARGE:
      return "Fast Charge";
    case CHARGE_STATUS_CHARGE_DONE:
      return "Charge Done";
    default:
      return "Unknown";
  }
}

const char *SY6970Component::get_ntc_status_string() {
  uint8_t status = this->get_ntc_status();
  switch (status) {
    case 0:
      return "Normal";
    case 2:
      return "Warm";
    case 3:
      return "Cool";
    case 5:
      return "Cold";
    case 6:
      return "Hot";
    default:
      return "Unknown";
  }
}

uint8_t SY6970Component::get_bus_status() {
  if (!this->initialized_)
    return 0;

  uint8_t reg_value;
  if (!this->read_register_(SY6970_REG_0B, &reg_value)) {
    return 0;
  }

  return (reg_value >> 5) & 0x07;
}

uint8_t SY6970Component::get_charge_status() {
  if (!this->initialized_)
    return 0;

  uint8_t reg_value;
  if (!this->read_register_(SY6970_REG_0B, &reg_value)) {
    return 0;
  }

  return (reg_value >> 3) & 0x03;
}

uint8_t SY6970Component::get_ntc_status() {
  if (!this->initialized_)
    return 0;

  uint8_t reg_value;
  if (!this->read_register_(SY6970_REG_0C, &reg_value)) {
    return 0;
  }

  return reg_value & 0x07;
}

void SY6970Component::set_input_current_limit(uint16_t milliamps) {
  if (!this->initialized_)
    return;

  if (milliamps < INPUT_CURRENT_MIN) {
    milliamps = INPUT_CURRENT_MIN;
  }

  uint8_t val = (milliamps - INPUT_CURRENT_MIN) / INPUT_CURRENT_STEP;
  if (val > 0x3F) {
    val = 0x3F;
  }

  this->update_register_(SY6970_REG_00, 0x3F, val);
}

void SY6970Component::set_charge_target_voltage(uint16_t millivolts) {
  if (!this->initialized_)
    return;

  if (millivolts < CHG_VOLTAGE_BASE) {
    millivolts = CHG_VOLTAGE_BASE;
  }

  uint8_t val = (millivolts - CHG_VOLTAGE_BASE) / CHG_VOLTAGE_STEP;
  if (val > 0x3F) {
    val = 0x3F;
  }

  this->update_register_(SY6970_REG_06, 0xFC, val << 2);
}

void SY6970Component::set_precharge_current(uint16_t milliamps) {
  if (!this->initialized_)
    return;

  if (milliamps < PRE_CHG_BASE) {
    milliamps = PRE_CHG_BASE;
  }

  uint8_t val = (milliamps - PRE_CHG_BASE) / PRE_CHG_STEP;
  if (val > 0x0F) {
    val = 0x0F;
  }

  this->update_register_(SY6970_REG_05, 0xF0, val << 4);
}

void SY6970Component::set_charge_current(uint16_t milliamps) {
  if (!this->initialized_)
    return;

  uint8_t val = milliamps / 64;
  if (val > 0x7F) {
    val = 0x7F;
  }

  this->update_register_(SY6970_REG_04, 0x7F, val);
}

void SY6970Component::enable_charge() {
  if (!this->initialized_)
    return;

  this->update_register_(SY6970_REG_03, 0x10, 0x10);
}

void SY6970Component::disable_charge() {
  if (!this->initialized_)
    return;

  this->update_register_(SY6970_REG_03, 0x10, 0x00);
}

void SY6970Component::enable_status_led() {
  if (!this->initialized_)
    return;

  // Clear bit 6 to enable LED
  this->update_register_(SY6970_REG_07, 0x40, 0x00);
}

void SY6970Component::disable_status_led() {
  if (!this->initialized_)
    return;

  // Set bit 6 to disable LED
  this->update_register_(SY6970_REG_07, 0x40, 0x40);
}

void SY6970Component::enable_adc_measure() {
  if (!this->initialized_)
    return;

  // Set bits to enable ADC conversion
  this->update_register_(SY6970_REG_02, 0xC0, 0xC0);
}

uint16_t SY6970Component::get_charge_target_voltage() {
  if (!this->initialized_)
    return 0;

  uint8_t reg_value;
  if (!this->read_register_(SY6970_REG_06, &reg_value)) {
    return 0;
  }

  uint8_t val = (reg_value >> 2) & 0x3F;
  return CHG_VOLTAGE_BASE + (val * CHG_VOLTAGE_STEP);
}

uint16_t SY6970Component::get_charge_constant_current() {
  if (!this->initialized_)
    return 0;

  uint8_t reg_value;
  if (!this->read_register_(SY6970_REG_04, &reg_value)) {
    return 0;
  }

  uint8_t val = reg_value & 0x7F;
  return val * 64;
}

}  // namespace sy6970
}  // namespace esphome
