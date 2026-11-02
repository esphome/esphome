#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/i2c/i2c_bus.h"

namespace esphome::kmeteriso {

/// This class implements support for the KMeterISO thermocouple sensor.
class KMeterISOComponent final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(temperature)
  SUB_SENSOR(internal_temperature)

 public:
  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void setup() override;
  void update() override;

 protected:
  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
    STATUS_FAILED,
  } error_code_{NONE};
};

}  // namespace esphome::kmeteriso
