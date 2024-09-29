#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

#include <vector>

namespace esphome {
namespace daly_hkms_bms {

class DalyHkmsBmsComponent : public PollingComponent, public modbus::ModbusDevice {
 public:
  void loop() override;
  void update() override;
  void on_modbus_data(const std::vector<uint8_t> &data) override;
  void dump_config() override;

  void set_daly_address(uint8_t address);

#ifdef USE_SENSOR
  SUB_SENSOR(voltage)
  SUB_SENSOR(current)
  SUB_SENSOR(battery_level)
#endif

 protected:
  bool waiting_to_update_;
  uint32_t last_send_;
  uint8_t daly_address_;

};

}  // namespace daly_hkms_bms
}  // namespace esphome
