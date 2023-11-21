#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

#include <vector>

namespace esphome {
namespace selec_meter {

#define SELEC_METER_SENSOR(name) \
 protected: \
  sensor::Sensor *name##_sensor_{nullptr}; \
\
 public: \
  void set_##name##_sensor(sensor::Sensor *(name)) { this->name##_sensor_ = name; }

class SelecMeter : public PollingComponent, public modbus::ModbusDevice {
 public:
  SELEC_METER_SENSOR(total_active_energy)
  SELEC_METER_SENSOR(import_active_energy)
  SELEC_METER_SENSOR(export_active_energy)
  SELEC_METER_SENSOR(total_reactive_energy)
  SELEC_METER_SENSOR(import_reactive_energy)
  SELEC_METER_SENSOR(export_reactive_energy)
  SELEC_METER_SENSOR(apparent_energy)
  SELEC_METER_SENSOR(active_power)
  SELEC_METER_SENSOR(reactive_power)
  SELEC_METER_SENSOR(apparent_power)
  SELEC_METER_SENSOR(voltage)
  SELEC_METER_SENSOR(current)
  SELEC_METER_SENSOR(power_factor)
  SELEC_METER_SENSOR(frequency)
  SELEC_METER_SENSOR(maximum_demand_active_power)
  SELEC_METER_SENSOR(maximum_demand_reactive_power)
  SELEC_METER_SENSOR(maximum_demand_apparent_power)

  void update() override;

  void on_modbus_data(const std::vector<uint8_t> &data) override;

  void dump_config() override;
};

}  // namespace selec_meter
}  // namespace esphome
