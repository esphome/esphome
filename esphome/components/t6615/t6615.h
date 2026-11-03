#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome::t6615 {

enum class T6615Command : uint8_t {
  NONE = 0,
  GET_PPM,
  GET_SERIAL,
  GET_VERSION,
  GET_ELEVATION,
  GET_ABC,
  ENABLE_ABC,
  DISABLE_ABC,
  SET_ELEVATION,
};

class T6615Component final : public PollingComponent, public uart::UARTDevice {
  SUB_SENSOR(co2)

 public:
  void loop() override;
  void update() override;
  void dump_config() override;

 protected:
  void query_ppm_();
  void send_ppm_command_();

  T6615Command command_ = T6615Command::NONE;
  uint32_t command_time_ = 0;
};

}  // namespace esphome::t6615
