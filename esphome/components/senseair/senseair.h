#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome::senseair {

enum SenseAirStatus : uint8_t {
  FATAL_ERROR = 1 << 0,
  OFFSET_ERROR = 1 << 1,
  ALGORITHM_ERROR = 1 << 2,
  OUTPUT_ERROR = 1 << 3,
  SELF_DIAGNOSTIC_ERROR = 1 << 4,
  OUT_OF_RANGE_ERROR = 1 << 5,
  MEMORY_ERROR = 1 << 6,
  RESERVED = 1 << 7
};

class SenseAirComponent final : public PollingComponent, public uart::UARTDevice {
  SUB_SENSOR(co2)

 public:
  void update() override;
  void dump_config() override;

  void background_calibration();
  void background_calibration_result();
  void abc_get_period();
  void abc_enable();
  void abc_disable();

 protected:
  bool senseair_write_command_(const uint8_t *command, uint8_t *response, uint8_t response_length);
};

}  // namespace esphome::senseair
