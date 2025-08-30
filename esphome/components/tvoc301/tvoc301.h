#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace tvoc301 {

class TVOC301Component : public PollingComponent, public uart::UARTDevice {
 public:
  TVOC301Component() = default;

  void set_eco2_sensor(sensor::Sensor *eco2_sensor) { this->eco2_sensor_ = eco2_sensor; }
  void set_tvoc_sensor(sensor::Sensor *tvoc_sensor) { this->tvoc_sensor_ = tvoc_sensor; }
  void set_ch2o_sensor(sensor::Sensor *ch2o_sensor) { this->ch2o_sensor_ = ch2o_sensor; }
  void setup() override;
  void dump_config() override;
  void loop() override;
  void update() override;

  float get_setup_priority() const override;

 protected:
  optional<bool> check_byte_() const;
  void parse_data_();
  uint8_t tvoc301_checksum_() const;
  uint16_t get_16_bit_uint_(uint8_t start_index) const {
    return encode_uint16(this->data_[start_index], this->data_[start_index + 1]);
  }

  sensor::Sensor *eco2_sensor_{nullptr};
  sensor::Sensor *tvoc_sensor_{nullptr};
  sensor::Sensor *ch2o_sensor_{nullptr};

  uint8_t data_[20];
  uint8_t data_index_{0};
  uint32_t last_transmission_{0};
};

}  // namespace tvoc301
}  // namespace esphome
