#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace pmsx003 {

enum PMSX003Type {
  PMSX003_TYPE_X003 = 0,
  PMSX003_TYPE_5003T,
  PMSX003_TYPE_5003ST,
  PMSX003_TYPE_5003S,
};

#define PMSX003_SENSOR(key) \
 protected: \
  sensor::Sensor *key##_; \
\
 public: \
  void set_##key##_sensor(sensor::Sensor *sensor) { this->key##_ = sensor; }

class PMSX003Component : public uart::UARTDevice, public Component {
 public:
  PMSX003Component() = default;
  void loop() override;
  float get_setup_priority() const override;
  void dump_config() override;

  void set_type(PMSX003Type type) { type_ = type; }

  // "Standard Particle"
  PMSX003_SENSOR(pm_1_0_std)
  PMSX003_SENSOR(pm_2_5_std)
  PMSX003_SENSOR(pm_10_0_std)

  // "Under Atmospheric Pressure"
  PMSX003_SENSOR(pm_1_0)
  PMSX003_SENSOR(pm_2_5)
  PMSX003_SENSOR(pm_10_0)

  // Particle counts by size
  PMSX003_SENSOR(pm_0_3um)
  PMSX003_SENSOR(pm_0_5um)
  PMSX003_SENSOR(pm_1_0um)
  PMSX003_SENSOR(pm_2_5um)
  PMSX003_SENSOR(pm_5_0um)
  PMSX003_SENSOR(pm_10_0um)

  PMSX003_SENSOR(temperature)
  PMSX003_SENSOR(humidity)
  PMSX003_SENSOR(formaldehyde)

 protected:
  optional<bool> check_byte_();
  void parse_data_();
  uint16_t get_16_bit_uint_(uint8_t start_index);

  uint8_t data_[64];
  uint8_t data_index_{0};
  uint32_t last_transmission_{0};
  PMSX003Type type_;
};

}  // namespace pmsx003
}  // namespace esphome
