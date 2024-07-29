#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/bedjet/bedjet_child.h"
#include "esphome/components/bedjet/bedjet_codec.h"

namespace esphome {
namespace bedjet {

class BedjetSensor : public BedJetClient, public Component {
 public:
  void dump_config() override;

  void on_status(const BedjetStatusPacket *data) override;
  void on_bedjet_state(bool is_ready) override;
  std::string describe() override;

  void set_outlet_temperature_sensor(sensor::Sensor *outlet_temperature_sensor) {
    this->outlet_temperature_sensor_ = outlet_temperature_sensor;
  }
  void set_ambient_temperature_sensor(sensor::Sensor *ambient_temperature_sensor) {
    this->ambient_temperature_sensor_ = ambient_temperature_sensor;
  }

 protected:
  sensor::Sensor *outlet_temperature_sensor_{nullptr};
  sensor::Sensor *ambient_temperature_sensor_{nullptr};
};

}  // namespace bedjet
}  // namespace esphome
