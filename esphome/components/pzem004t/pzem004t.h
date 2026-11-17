#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::pzem004t {

class PZEM004T final : public PollingComponent, public uart::UARTDevice {
  SUB_SENSOR(voltage)
  SUB_SENSOR(current)
  SUB_SENSOR(power)
  SUB_SENSOR(energy)

 public:
  void setup() override;

  void loop() override;

  void update() override;

  void dump_config() override;

 protected:
  enum PZEM004TReadState {
    SET_ADDRESS = 0xB4,
    READ_VOLTAGE = 0xB0,
    READ_CURRENT = 0xB1,
    READ_POWER = 0xB2,
    READ_ENERGY = 0xB3,
    DONE = 0x00,
  } read_state_{DONE};

  void write_state_(PZEM004TReadState state);

  uint32_t last_read_{0};
};

}  // namespace esphome::pzem004t
