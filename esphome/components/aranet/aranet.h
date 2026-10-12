#pragma once

#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::aranet {

class Aranet final : public Component, public ble_device_base::ESPBTDeviceListener {
 public:
  explicit Aranet(uint64_t address) : address_(address) {}

  bool parse_device(const ble_device_base::ESPBTDevice &device) override;
  void dump_config() override;

 protected:
  static constexpr uint8_t WARNED_INVALID_LENGTH = 1 << 0;
  static constexpr uint8_t WARNED_INTEGRATION_DISABLED = 1 << 1;
  static constexpr uint8_t WARNED_UNSUPPORTED_TYPE = 1 << 2;

  // Small state first so it fills the padding before the first pointer
  uint16_t last_measurement_counter_{0x100};  // Out of range of the 8-bit counter until the first measurement
  uint8_t warned_{0};

  SUB_SENSOR(co2)
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)
  SUB_SENSOR(pressure)
  SUB_SENSOR(battery_level)
  SUB_SENSOR(measurement_interval)
  SUB_SENSOR(measurement_age)
  SUB_SENSOR(signal_strength)
  SUB_SENSOR(radon)
  SUB_SENSOR(radiation_rate)
  SUB_SENSOR(radiation_total)
  SUB_SENSOR(radiation_duration)

 protected:
  uint64_t address_;
};

}  // namespace esphome::aranet
