#pragma once

#include "../ebus_component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace ebus {

class EbusSensor : public EbusItem, public sensor::Sensor {
 public:
  EbusSensor() {}

  void set_response_read_bytes(uint8_t response_bytes) { this->response_bytes_ = response_bytes; }
  void set_response_read_divider(float response_divider) { this->response_divider_ = response_divider; }

  void process_received(Telegram /*telegram*/) override;

  float to_float(Telegram &telegram, uint8_t start, uint8_t length, float divider);

 protected:
  uint8_t response_bytes_;
  float response_divider_;
};

}  // namespace ebus
}  // namespace esphome
