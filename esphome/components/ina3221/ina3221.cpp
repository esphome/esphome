#include "ina3221.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome::ina3221 {

ESPHOME_LOG_TAG(TAG, "ina3221");

static const uint8_t INA3221_REGISTER_CONFIG = 0x00;
static const uint8_t INA3221_REGISTER_CHANNEL1_SHUNT_VOLTAGE = 0x01;
static const uint8_t INA3221_REGISTER_CHANNEL1_BUS_VOLTAGE = 0x02;
static const uint8_t INA3221_REGISTER_CHANNEL2_SHUNT_VOLTAGE = 0x03;
static const uint8_t INA3221_REGISTER_CHANNEL2_BUS_VOLTAGE = 0x04;
static const uint8_t INA3221_REGISTER_CHANNEL3_SHUNT_VOLTAGE = 0x05;
static const uint8_t INA3221_REGISTER_CHANNEL3_BUS_VOLTAGE = 0x06;
#ifdef USE_INA3221_ALERT_LIMITS
static constexpr uint8_t INA3221_REGISTER_CHANNEL1_CRITICAL_ALERT = 0x07;
static constexpr uint8_t INA3221_REGISTER_CHANNEL1_WARNING_ALERT = 0x0A;
#endif
#ifdef USE_INA3221_SUMMATION
static constexpr uint8_t INA3221_REGISTER_SHUNT_VOLTAGE_SUM = 0x0D;
static constexpr uint8_t INA3221_REGISTER_MASK_ENABLE = 0x0F;
#endif

// Configuration register: bit 15 resets the chip, bits 14..12 enable channels 1..3 (the same bit
// positions select channels for summation in the mask/enable register) and bits 2..0 set the mode.
// Code generation fills in the averaging and conversion time fields.
static constexpr uint16_t INA3221_CONFIG_RESET = 0x8000;
static constexpr uint16_t INA3221_CONFIG_CHANNEL1_ENABLE = 0x4000;
static constexpr uint16_t INA3221_CONFIG_CHANNEL_MASK = 0x7000;
static constexpr uint16_t INA3221_CONFIG_MODE_MASK = 0x0007;

static constexpr uint32_t READ_TIMEOUT_ID = 0;

// Addresses:
// A0 = GND -> 0x40
// A0 = VS  -> 0x41
// A0 = SDA -> 0x42
// A0 = SCL -> 0x43

void INA3221Component::setup() {
  if (!this->write_byte_16(INA3221_REGISTER_CONFIG, INA3221_CONFIG_RESET)) {
    this->mark_failed();
    return;
  }
  delay(1);

  if (!this->write_byte_16(INA3221_REGISTER_CONFIG, this->config_)) {
    this->mark_failed();
    return;
  }

#ifdef USE_INA3221_ALERT_LIMITS
  for (uint8_t i = 0; i < 3; i++) {
    const INA3221Channel &channel = this->channels_[i];
    if (channel.critical_limit_ != 0) {
      this->write_byte_16(INA3221_REGISTER_CHANNEL1_CRITICAL_ALERT + i, channel.critical_limit_);
    }
    if (channel.warning_limit_ != 0) {
      this->write_byte_16(INA3221_REGISTER_CHANNEL1_WARNING_ALERT + i, channel.warning_limit_);
    }
  }
#endif

#ifdef USE_INA3221_SUMMATION
  if (this->sum_shunt_voltage_sensor_ != nullptr) {
    // Sum the shunt voltages of every enabled channel
    this->write_byte_16(INA3221_REGISTER_MASK_ENABLE, this->config_ & INA3221_CONFIG_CHANNEL_MASK);
  }
#endif
}

void INA3221Component::dump_config() {
  ESP_LOGCONFIG(TAG, "INA3221:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Config register: 0x%04X", this->config_);
  if (this->single_shot_wait_ms_ != 0) {
    ESP_LOGCONFIG(TAG, "  Single-shot conversion: %u ms", this->single_shot_wait_ms_);
  }

  LOG_SENSOR("  ", "Bus Voltage #1", this->channels_[0].bus_voltage_sensor_);
  LOG_SENSOR("  ", "Shunt Voltage #1", this->channels_[0].shunt_voltage_sensor_);
  LOG_SENSOR("  ", "Current #1", this->channels_[0].current_sensor_);
  LOG_SENSOR("  ", "Power #1", this->channels_[0].power_sensor_);
  LOG_SENSOR("  ", "Bus Voltage #2", this->channels_[1].bus_voltage_sensor_);
  LOG_SENSOR("  ", "Shunt Voltage #2", this->channels_[1].shunt_voltage_sensor_);
  LOG_SENSOR("  ", "Current #2", this->channels_[1].current_sensor_);
  LOG_SENSOR("  ", "Power #2", this->channels_[1].power_sensor_);
  LOG_SENSOR("  ", "Bus Voltage #3", this->channels_[2].bus_voltage_sensor_);
  LOG_SENSOR("  ", "Shunt Voltage #3", this->channels_[2].shunt_voltage_sensor_);
  LOG_SENSOR("  ", "Current #3", this->channels_[2].current_sensor_);
  LOG_SENSOR("  ", "Power #3", this->channels_[2].power_sensor_);
#ifdef USE_INA3221_SUMMATION
  LOG_SENSOR("  ", "Sum Shunt Voltage", this->sum_shunt_voltage_sensor_);
  LOG_SENSOR("  ", "Sum Current", this->sum_current_sensor_);
  LOG_SENSOR("  ", "Sum Power", this->sum_power_sensor_);
#endif
}

inline uint8_t ina3221_bus_voltage_register(int channel) { return 0x02 + channel * 2; }

inline uint8_t ina3221_shunt_voltage_register(int channel) { return 0x01 + channel * 2; }

void INA3221Component::update() {
  if (this->single_shot_wait_ms_ == 0) {
    this->read_data_();
    return;
  }
  // In single-shot mode, writing the configuration register starts one conversion of every channel
  if (!this->write_byte_16(INA3221_REGISTER_CONFIG, this->config_)) {
    this->status_set_warning();
    return;
  }
  this->set_timeout(READ_TIMEOUT_ID, this->single_shot_wait_ms_, [this]() { this->read_data_(); });
}

void INA3221Component::read_data_() {
#ifdef USE_INA3221_SUMMATION
  // The software sums need every enabled channel's readings, not only the ones with their own sensors
  const bool sum_all = this->sum_current_sensor_ != nullptr || this->sum_power_sensor_ != nullptr;
  float total_current_a = 0.0f;
  float total_power_w = 0.0f;
#else
  constexpr bool sum_all = false;
#endif

  for (int i = 0; i < 3; i++) {
    if ((this->config_ & (INA3221_CONFIG_CHANNEL1_ENABLE >> i)) == 0) {
      continue;
    }
    INA3221Channel &channel = this->channels_[i];
    float bus_voltage_v = NAN, current_a = NAN;
    uint16_t raw;
    if (sum_all || channel.should_measure_bus_voltage()) {
      if (!this->read_byte_16(ina3221_bus_voltage_register(i), &raw)) {
        this->status_set_warning();
        return;
      }
      bus_voltage_v = int16_t(raw) / 1000.0f;
      if (channel.bus_voltage_sensor_ != nullptr)
        channel.bus_voltage_sensor_->publish_state(bus_voltage_v);
    }
    if (sum_all || channel.should_measure_shunt_voltage()) {
      if (!this->read_byte_16(ina3221_shunt_voltage_register(i), &raw)) {
        this->status_set_warning();
        return;
      }
      const float shunt_voltage_v = int16_t(raw) * 40.0f / 8.0f / 1000000.0f;
      if (channel.shunt_voltage_sensor_ != nullptr)
        channel.shunt_voltage_sensor_->publish_state(shunt_voltage_v);
      current_a = shunt_voltage_v / channel.shunt_resistance_;
      if (channel.current_sensor_ != nullptr)
        channel.current_sensor_->publish_state(current_a);
    }
    if (channel.power_sensor_ != nullptr) {
      channel.power_sensor_->publish_state(bus_voltage_v * current_a);
    }
#ifdef USE_INA3221_SUMMATION
    if (sum_all) {
      total_current_a += current_a;
      total_power_w += bus_voltage_v * current_a;
    }
#endif
  }

#ifdef USE_INA3221_SUMMATION
  if (this->sum_shunt_voltage_sensor_ != nullptr) {
    uint16_t raw;
    if (!this->read_byte_16(INA3221_REGISTER_SHUNT_VOLTAGE_SUM, &raw)) {
      this->status_set_warning();
      return;
    }
    // 40 uV per step, held in bits 15..1
    this->sum_shunt_voltage_sensor_->publish_state(int16_t(raw) * 40.0f / 2.0f / 1000000.0f);
  }
  if (this->sum_current_sensor_ != nullptr) {
    this->sum_current_sensor_->publish_state(total_current_a);
  }
  if (this->sum_power_sensor_ != nullptr) {
    this->sum_power_sensor_->publish_state(total_power_w);
  }
#endif
}

void INA3221Component::on_powerdown() {
  // Mode 0 powers the chip down; setup() resets it on the next boot
  this->write_byte_16(INA3221_REGISTER_CONFIG, static_cast<uint16_t>(this->config_ & ~INA3221_CONFIG_MODE_MASK));
}

void INA3221Component::set_shunt_resistance(int channel, float resistance_ohm) {
  this->channels_[channel].shunt_resistance_ = resistance_ohm;
}

bool INA3221Component::INA3221Channel::should_measure_shunt_voltage() {
  return this->shunt_voltage_sensor_ != nullptr || this->current_sensor_ != nullptr || this->power_sensor_ != nullptr;
}
bool INA3221Component::INA3221Channel::should_measure_bus_voltage() {
  return this->bus_voltage_sensor_ != nullptr || this->power_sensor_ != nullptr;
}

}  // namespace esphome::ina3221
