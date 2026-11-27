#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

#include <span>

namespace esphome::pzemac {

class PZEMAC final : public PollingComponent, public modbus::ModbusClientDevice {
 public:
  SUB_SENSOR(voltage)
  SUB_SENSOR(current)
  SUB_SENSOR(power)
  SUB_SENSOR(energy)
  SUB_SENSOR(frequency)
  SUB_SENSOR(power_factor)

  void update() override;

  void on_read_input_registers(uint16_t start_address, std::span<const uint16_t> registers,
                               modbus::ResponseStatus status) override;
  void on_custom_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu,
                          modbus::ResponseStatus status) override;

  void dump_config() override;
  void reset_energy();
};

}  // namespace esphome::pzemac
