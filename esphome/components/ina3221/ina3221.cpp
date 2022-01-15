#include "ina3221.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ina3221 {

static const char *const TAG = "ina3221";

static const uint8_t INA3221_REGISTER_CONFIG = 0x00;
static const uint8_t INA3221_REGISTER_CHANNEL1_SHUNT_VOLTAGE = 0x01;
static const uint8_t INA3221_REGISTER_CHANNEL1_BUS_VOLTAGE = 0x02;
static const uint8_t INA3221_REGISTER_CHANNEL2_SHUNT_VOLTAGE = 0x03;
static const uint8_t INA3221_REGISTER_CHANNEL2_BUS_VOLTAGE = 0x04;
static const uint8_t INA3221_REGISTER_CHANNEL3_SHUNT_VOLTAGE = 0x05;
static const uint8_t INA3221_REGISTER_CHANNEL3_BUS_VOLTAGE = 0x06;

// Addresses:
// A0 = GND -> 0x40
// A0 = VS  -> 0x41
// A0 = SDA -> 0x42
// A0 = SCL -> 0x43

void INA3221Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up INA3221...");
  // Config Register
  // 0bx000000000000000 << 15 RESET Bit (1 -> trigger reset)
  if (!this->write_byte_16(INA3221_REGISTER_CONFIG, 0x8000)) {
    this->mark_failed();
    return;
  }
  delay(1);

  uint16_t config = 0;
  // 0b0xxx000000000000 << 12 Channel Enables (1 -> ON)
  if (this->channels_[0].exists()) {
    config |= 0b0100000000000000;
  }
  if (this->channels_[1].exists()) {
    config |= 0b0010000000000000;
  }
  if (this->channels_[2].exists()) {
    config |= 0b0001000000000000;
  }
  // 0b0000xxx000000000 << 9 Averaging Mode (0 -> 1 sample, 111 -> 1024 samples)
  config |= 0b0000000000000000;
  // 0b0000000xxx000000 << 6 Bus Voltage Conversion time (100 -> 1.1ms, 111 -> 8.244 ms)
  config |= 0b0000000111000000;
  // 0b0000000000xxx000 << 3 Shunt Voltage Conversion time (same as above)
  config |= 0b0000000000111000;
  // 0b0000000000000xxx << 0 Operating mode (111 -> Shunt and bus, continuous)
  config |= 0b0000000000000111;
  if (!this->write_byte_16(INA3221_REGISTER_CONFIG, config)) {
    this->mark_failed();
    return;
  }
}

void INA3221Component::dump_config() {
  ESP_LOGCONFIG(TAG, "INA3221:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with INA3221 failed!");
  }
  LOG_UPDATE_INTERVAL(this);

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
}

inline uint8_t ina3221_bus_voltage_register(int channel) { return 0x02 + channel * 2; }

inline uint8_t ina3221_shunt_voltage_register(int channel) { return 0x01 + channel * 2; }

void INA3221Component::update() {
  for (int i = 0; i < 3; i++) {
    INA3221Channel &channel = this->channels_[i];
    float bus_voltage_v = NAN, current_a = NAN;
    uint16_t raw;
    if (channel.should_measure_bus_voltage()) {
      if (!this->read_byte_16(ina3221_bus_voltage_register(i), &raw)) {
        this->status_set_warning();
        return;
      }
      bus_voltage_v = int16_t(raw) / 1000.0f;
      if (channel.bus_voltage_sensor_ != nullptr)
        channel.bus_voltage_sensor_->publish_state(bus_voltage_v);
    }
    if (channel.should_measure_shunt_voltage()) {
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
  }
}

float INA3221Component::get_setup_priority() const { return setup_priority::DATA; }
void INA3221Component::set_shunt_resistance(int channel, float resistance_ohm) {
  this->channels_[channel].shunt_resistance_ = resistance_ohm;
}

bool INA3221Component::INA3221Channel::exists() {
  return this->bus_voltage_sensor_ != nullptr || this->shunt_voltage_sensor_ != nullptr ||
         this->current_sensor_ != nullptr || this->power_sensor_ != nullptr;
}
bool INA3221Component::INA3221Channel::should_measure_shunt_voltage() {
  return this->shunt_voltage_sensor_ != nullptr || this->current_sensor_ != nullptr || this->power_sensor_ != nullptr;
}
bool INA3221Component::INA3221Channel::should_measure_bus_voltage() {
  return this->bus_voltage_sensor_ != nullptr || this->power_sensor_ != nullptr;
}

}  // namespace ina3221
}  // namespace esphome
