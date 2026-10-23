#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#include "../duco.h"

namespace esphome::duco {

enum class DucoValueFormat : uint8_t {
  DUCO_VALUE_FORMAT_UINT8,
  DUCO_VALUE_FORMAT_UINT16,
  DUCO_VALUE_FORMAT_INT16,
};

/// One value read from the box: a request, then a little endian field at offset in the answer.
/// Values outside [minimum, maximum] are dropped, the box reports those for absent sensors.
class DucoSensor : public DucoDevice, public PollingComponent, public sensor::Sensor {
 public:
  DucoSensor(uint8_t function, uint8_t argument, uint8_t offset, DucoValueFormat format, uint8_t divisor,
             int16_t minimum, uint16_t maximum)
      : function_(function),
        argument_(argument),
        offset_(offset),
        format_(format),
        divisor_(divisor),
        minimum_(minimum),
        maximum_(maximum) {}

  void update() override;
  bool receive_response(const DucoMessage &message) override;

 protected:
  uint8_t function_;
  uint8_t argument_;  // node address, parameter number or node, by function
  uint8_t offset_;
  DucoValueFormat format_;
  uint8_t divisor_;
  int16_t minimum_;
  uint16_t maximum_;
};

}  // namespace esphome::duco
