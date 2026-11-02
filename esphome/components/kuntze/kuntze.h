#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

#include <span>

namespace esphome::kuntze {

class Kuntze final : public PollingComponent, public modbus::ModbusClientDevice {
  SUB_SENSOR(ph)
  SUB_SENSOR(temperature)
  SUB_SENSOR(dis1)
  SUB_SENSOR(dis2)
  SUB_SENSOR(redox)
  SUB_SENSOR(ec)
  SUB_SENSOR(oci)

 public:
  void update() override;

  void on_read_holding_registers(uint16_t start_address, std::span<const uint16_t> registers,
                                 modbus::ResponseStatus status) override;

  void dump_config() override;
};

}  // namespace esphome::kuntze
