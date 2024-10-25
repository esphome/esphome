#pragma once

#include "../ebus_component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace ebus {

class EbusBinarySensor : public EbusItem, public binary_sensor::BinarySensor {
 public:
  EbusBinarySensor() {}

  void set_response_read_mask(uint8_t mask) { this->mask_ = mask; };

  void process_received(Telegram /*telegram*/) override;

 protected:
  int8_t mask_;
};

}  // namespace ebus
}  // namespace esphome
