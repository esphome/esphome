#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

namespace esphome {
namespace kuntze {

class Kuntze : public PollingComponent, public modbus::ModbusDevice {
 public:
  void set_ph_sensor(sensor::Sensor *ph_sensor) { ph_sensor_ = ph_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_dis1_sensor(sensor::Sensor *dis1_sensor) { dis1_sensor_ = dis1_sensor; }
  void set_dis2_sensor(sensor::Sensor *dis2_sensor) { dis2_sensor_ = dis2_sensor; }
  void set_redox_sensor(sensor::Sensor *redox_sensor) { redox_sensor_ = redox_sensor; }
  void set_ec_sensor(sensor::Sensor *ec_sensor) { ec_sensor_ = ec_sensor; }
  void set_oci_sensor(sensor::Sensor *oci_sensor) { oci_sensor_ = oci_sensor; }

  void loop() override;
  void update() override;

  void on_modbus_data(const std::vector<uint8_t> &data) override;

  void dump_config() override;

 protected:
  int state_{0};
  bool waiting_{false};
  uint32_t last_send_{0};

  sensor::Sensor *ph_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *dis1_sensor_{nullptr};
  sensor::Sensor *dis2_sensor_{nullptr};
  sensor::Sensor *redox_sensor_{nullptr};
  sensor::Sensor *ec_sensor_{nullptr};
  sensor::Sensor *oci_sensor_{nullptr};
};

}  // namespace kuntze
}  // namespace esphome
