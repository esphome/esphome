#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace ade7953 {

class ADE7953 : public i2c::I2CDevice, public PollingComponent{
 public:
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) {
    voltage_sensor_ = voltage_sensor;
  }
  void set_current_a_sensor(sensor::Sensor *current_a_sensor) {
    current_a_sensor_ = current_a_sensor;
  }
  void set_current_b_sensor(sensor::Sensor *current_b_sensor) {
    current_b_sensor_ = current_b_sensor;
  }
  void set_active_power_a_sensor(sensor::Sensor *active_power_a_sensor) {
    active_power_a_sensor_ = active_power_a_sensor;
  }
  void set_active_power_b_sensor(sensor::Sensor *active_power_b_sensor) {
    active_power_b_sensor_ = active_power_b_sensor;
  }

  void setup() override {
    this->set_timeout(100, [this]() {
      this->ade_write_<uint8_t>(0x0010, 0x04);
      this->ade_write_<uint8_t>(0x00FE, 0xAD);
      this->ade_write_<uint16_t>(0x0120, 0x0030);
      this->is_setup_ = true;
    });
  }

  void dump_config() override;

#define ADE_PUBLISH_(name, factor) \
  if (name) { \
    float value = *name / factor; \
    this-> name ## _sensor_->publish_state(value); \
  }
#define ADE_PUBLISH(name, factor) ADE_PUBLISH_(name, factor)

  void update() override {
    if (!this->is_setup_)
      return;

    auto active_power_a = this->ade_read_<int32_t>(0x0312);
    ADE_PUBLISH(active_power_a, 154.0f);
    auto active_power_b = this->ade_read_<int32_t>(0x0313);
    ADE_PUBLISH(active_power_b, 154.0f);
    auto current_a = this->ade_read_<uint32_t>(0x031A);
    ADE_PUBLISH(current_a, 100000.0f);
    auto current_b = this->ade_read_<uint32_t>(0x031B);
    ADE_PUBLISH(current_b, 100000.0f);
    auto voltage = this->ade_read_<uint32_t>(0x031C);
    ADE_PUBLISH(voltage, 26000.0f);

//    auto apparent_power_a = this->ade_read_<int32_t>(0x0310);
//    auto apparent_power_b = this->ade_read_<int32_t>(0x0311);
//    auto reactive_power_a = this->ade_read_<int32_t>(0x0314);
//    auto reactive_power_b = this->ade_read_<int32_t>(0x0315);
//    auto power_factor_a = this->ade_read_<int16_t>(0x010A);
//    auto power_factor_b = this->ade_read_<int16_t>(0x010B);
  }
 protected:
  template<typename T>
  bool ade_write_(uint16_t reg, T value) {
    std::vector<uint8_t> data;
    data.push_back(reg >> 8);
    data.push_back(reg >> 0);
    for (int i = sizeof(T) - 1; i >= 0; i--)
      data.push_back(value >> (i * 8));
    return this->write_bytes_raw(data);
  }
  template<typename T>
  optional<T> ade_read_(uint16_t reg) {
    uint8_t hi = reg >> 8;
    uint8_t lo = reg >> 0;
    if (!this->write_bytes_raw({hi, lo}))
      return {};
    auto ret = this->read_bytes_raw<sizeof(T)>();
    if (!ret.has_value())
      return {};
    T result = 0;
    for (int i = 0, j = sizeof(T) - 1; i < sizeof(T); i++, j--)
      result |= T((*ret)[i]) << (j * 8);
    return result;
  }

  bool is_setup_{false};
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *current_a_sensor_{nullptr};
  sensor::Sensor *current_b_sensor_{nullptr};
  sensor::Sensor *active_power_a_sensor_{nullptr};
  sensor::Sensor *active_power_b_sensor_{nullptr};
};

}  // namespace ade7953
}  // namespace esphome
