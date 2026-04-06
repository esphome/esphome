#pragma once

#include "../systa_bus.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::systa_bus {

class SystaSolarAquaSensor : public SystaBusListener, public Component {
 public:
  void dump_config() override;
  void set_temperature_tsa_sensor(sensor::Sensor *sensor) { this->temperature_tsa_sensor_ = sensor; }
  void set_temperature_tse_sensor(sensor::Sensor *sensor) { this->temperature_tse_sensor_ = sensor; }
  void set_temperature_twu_sensor(sensor::Sensor *sensor) { this->temperature_twu_sensor_ = sensor; }
  void set_temperature_tw2_sensor(sensor::Sensor *sensor) { this->temperature_tw2_sensor_ = sensor; }
  void set_pump_speed_sensor(sensor::Sensor *sensor) { this->pump_speed_sensor_ = sensor; }
  void handle_message(std::vector<uint8_t> &message) override;

 protected:
  sensor::Sensor *temperature_tsa_sensor_{nullptr};
  sensor::Sensor *temperature_tse_sensor_{nullptr};
  sensor::Sensor *temperature_twu_sensor_{nullptr};
  sensor::Sensor *temperature_tw2_sensor_{nullptr};
  sensor::Sensor *pump_speed_sensor_{nullptr};
};

}  // namespace esphome::systa_bus
