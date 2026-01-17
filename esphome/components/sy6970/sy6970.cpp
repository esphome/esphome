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

  this->initialized_ = true;
  ESP_LOGCONFIG(TAG, "SY6970 initialized successfully");
}

void SY6970Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SY6970:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with SY6970 failed!");
  }
  LOG_SENSOR("  ", "VBUS Voltage", this->vbus_voltage_sensor_);
  LOG_SENSOR("  ", "Battery Voltage", this->battery_voltage_sensor_);
  LOG_SENSOR("  ", "System Voltage", this->system_voltage_sensor_);
  LOG_SENSOR("  ", "Charge Current", this->charge_current_sensor_);
  LOG_SENSOR("  ", "Precharge Current", this->precharge_current_sensor_);
  LOG_BINARY_SENSOR("  ", "VBUS Connected", this->vbus_connected_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Charging", this->charging_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Charge Done", this->charge_done_binary_sensor_);
  LOG_TEXT_SENSOR("  ", "Bus Status", this->bus_status_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Charge Status", this->charge_status_text_sensor_);
  LOG_TEXT_SENSOR("  ", "NTC Status", this->ntc_status_text_sensor_);
}

void SY6970Component::update() {
  if (!this->initialized_) {
    return;
  }

  // Read all registers in one transaction
  if (!this->read_all_registers_()) {
    ESP_LOGW(TAG, "Failed to read registers during update");
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();

  // Publish all sensor values
  this->publish_sensors_(this->data_);
  this->publish_binary_sensors_(this->data_);
  this->publish_text_sensors_(this->data_);
}

void SY6970Component::publish_sensors_(const SY6970Data &data) {
  if (this->vbus_voltage_sensor_ != nullptr) {
    uint16_t vbus_mv = this->get_vbus_voltage_(data);
    this->vbus_voltage_sensor_->publish_state(vbus_mv / 1000.0f);
  }

  if (this->battery_voltage_sensor_ != nullptr) {
    uint16_t battery_mv = this->get_battery_voltage_(data);
    this->battery_voltage_sensor_->publish_state(battery_mv / 1000.0f);
  }

  if (this->system_voltage_sensor_ != nullptr) {
    uint16_t system_mv = this->get_system_voltage_(data);
    this->system_voltage_sensor_->publish_state(system_mv / 1000.0f);
  }

  if (this->charge_current_sensor_ != nullptr) {
    uint16_t charge_ma = this->get_charge_current_(data);
    this->charge_current_sensor_->publish_state(charge_ma);
  }

  if (this->precharge_current_sensor_ != nullptr) {
    uint16_t precharge_ma = this->get_precharge_current_(data);
    this->precharge_current_sensor_->publish_state(precharge_ma);
  }
}

void SY6970Component::publish_binary_sensors_(const SY6970Data &data) {
  if (this->vbus_connected_binary_sensor_ != nullptr) {
    bool vbus_connected = this->is_vbus_connected_(data);
    this->vbus_connected_binary_sensor_->publish_state(vbus_connected);
  }

  if (this->charging_binary_sensor_ != nullptr) {
    bool charging = this->is_charging_(data);
    this->charging_binary_sensor_->publish_state(charging);
  }

  if (this->charge_done_binary_sensor_ != nullptr) {
    bool charge_done = this->is_charge_done_(data);
    this->charge_done_binary_sensor_->publish_state(charge_done);
  }
}

void SY6970Component::publish_text_sensors_(const SY6970Data &data) {
  if (this->bus_status_text_sensor_ != nullptr) {
    uint8_t status = this->get_bus_status_(data);
    const char *status_str = this->get_bus_status_string_(status);
    this->bus_status_text_sensor_->publish_state(status_str);
  }

  if (this->charge_status_text_sensor_ != nullptr) {
    uint8_t status = this->get_charge_status_(data);
    const char *status_str = this->get_charge_status_string_(status);
    this->charge_status_text_sensor_->publish_state(status_str);
  }

  if (this->ntc_status_text_sensor_ != nullptr) {
    uint8_t status = this->get_ntc_status_(data);
    const char *status_str = this->get_ntc_status_string_(status);
    this->ntc_status_text_sensor_->publish_state(status_str);
  }
}

uint16_t SY6970Component::get_vbus_voltage_(const SY6970Data &data) {
  uint8_t vbus_val = data.registers[SY6970_REG_VBUS_VOLTAGE] & 0x7F;
  return VBUS_BASE + (vbus_val * VBUS_STEP);
}

uint16_t SY6970Component::get_battery_voltage_(const SY6970Data &data) {
  uint8_t vbat_val = data.registers[SY6970_REG_BATV] & 0x7F;
  return VBAT_BASE + (vbat_val * VBAT_STEP);
}

uint16_t SY6970Component::get_system_voltage_(const SY6970Data &data) {
  uint8_t vsys_val = data.registers[SY6970_REG_VINDPM_STATUS] & 0x7F;
  return VSYS_BASE + (vsys_val * VSYS_STEP);
}

uint16_t SY6970Component::get_charge_current_(const SY6970Data &data) {
  uint8_t ichg_val = data.registers[SY6970_REG_CHARGE_CURRENT_MONITOR] & 0x7F;
  return ichg_val * CHG_CURRENT_STEP;
}

uint16_t SY6970Component::get_precharge_current_(const SY6970Data &data) {
  uint8_t iprechg = (data.registers[SY6970_REG_PRECHARGE_CURRENT] >> 4) & 0x0F;
  return PRE_CHG_BASE + (iprechg * PRE_CHG_STEP);
}

bool SY6970Component::is_vbus_connected_(const SY6970Data &data) {
  uint8_t bus_status = this->get_bus_status_(data);
  return bus_status != BUS_STATUS_NO_INPUT;
}

bool SY6970Component::is_charging_(const SY6970Data &data) {
  uint8_t chrg_stat = this->get_charge_status_(data);
  return chrg_stat != CHARGE_STATUS_NOT_CHARGING && chrg_stat != CHARGE_STATUS_CHARGE_DONE;
}

bool SY6970Component::is_charge_done_(const SY6970Data &data) {
  uint8_t chrg_stat = this->get_charge_status_(data);
  return chrg_stat == CHARGE_STATUS_CHARGE_DONE;
}

uint8_t SY6970Component::get_bus_status_(const SY6970Data &data) {
  return (data.registers[SY6970_REG_STATUS] >> 5) & 0x07;
}

uint8_t SY6970Component::get_charge_status_(const SY6970Data &data) {
  return (data.registers[SY6970_REG_STATUS] >> 3) & 0x03;
}

uint8_t SY6970Component::get_ntc_status_(const SY6970Data &data) { return data.registers[SY6970_REG_FAULT] & 0x07; }

const char *SY6970Component::get_bus_status_string_(uint8_t status) {
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

const char *SY6970Component::get_charge_status_string_(uint8_t status) {
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

const char *SY6970Component::get_ntc_status_string_(uint8_t status) {
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

  this->update_register_(SY6970_REG_INPUT_CURRENT_LIMIT, 0x3F, val);
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

  this->update_register_(SY6970_REG_CHARGE_VOLTAGE, 0xFC, val << 2);
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

  this->update_register_(SY6970_REG_PRECHARGE_CURRENT, 0xF0, val << 4);
}

void SY6970Component::set_charge_current(uint16_t milliamps) {
  if (!this->initialized_)
    return;

  uint8_t val = milliamps / 64;
  if (val > 0x7F) {
    val = 0x7F;
  }

  this->update_register_(SY6970_REG_CHARGE_CURRENT, 0x7F, val);
}

void SY6970Component::enable_charge() {
  if (!this->initialized_)
    return;

  this->update_register_(SY6970_REG_SYS_CONTROL, 0x10, 0x10);
}

void SY6970Component::disable_charge() {
  if (!this->initialized_)
    return;

  this->update_register_(SY6970_REG_SYS_CONTROL, 0x10, 0x00);
}

void SY6970Component::enable_status_led() {
  if (!this->initialized_)
    return;

  // Clear bit 6 to enable LED
  this->update_register_(SY6970_REG_TIMER_CONTROL, 0x40, 0x00);
}

void SY6970Component::disable_status_led() {
  if (!this->initialized_)
    return;

  // Set bit 6 to disable LED
  this->update_register_(SY6970_REG_TIMER_CONTROL, 0x40, 0x40);
}

void SY6970Component::enable_adc_measure() {
  if (!this->initialized_)
    return;

  // Set bits to enable ADC conversion
  this->update_register_(SY6970_REG_ADC_CONTROL, 0xC0, 0xC0);
}

}  // namespace esphome::sy6970
