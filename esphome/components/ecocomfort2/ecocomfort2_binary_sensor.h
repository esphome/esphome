#pragma once
#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "ecocomfort2_child.h"
#include "ecocomfort2_hub.h"

namespace esphome {
namespace ecocomfort2 {

class Ecocomfort2BinarySensor : public Ecocomfort2Client, public Component {
 public:
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_connected_sensor(binary_sensor::BinarySensor *s) { this->connected_sensor_ = s; }
  void set_boost_sensor(binary_sensor::BinarySensor *s) { this->boost_sensor_ = s; }

  // Ecocomfort2Client callbacks
  void on_status() override;
  void on_config() override {}
  void on_connect(bool connected) override;
  const char *describe() const override { return "Ecocomfort2 Binary Sensor"; }

 protected:
  binary_sensor::BinarySensor *connected_sensor_{nullptr};
  binary_sensor::BinarySensor *boost_sensor_{nullptr};
};

}  // namespace ecocomfort2
}  // namespace esphome

#endif
