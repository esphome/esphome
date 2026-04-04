#pragma once
#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "ecocomfort2_child.h"
#include "ecocomfort2_hub.h"

namespace esphome {
namespace ecocomfort2 {

class Ecocomfort2Sensor : public Ecocomfort2Client, public Component {
 public:
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_temperature_sensor(sensor::Sensor *s) { this->temperature_sensor_ = s; }
  void set_humidity_sensor(sensor::Sensor *s) { this->humidity_sensor_ = s; }
  void set_voc_sensor(sensor::Sensor *s) { this->voc_sensor_ = s; }
  void set_direction_sensor(sensor::Sensor *s) { this->direction_sensor_ = s; }
  void set_actual_mode_sensor(sensor::Sensor *s) { this->actual_mode_sensor_ = s; }
  void set_actual_speed_sensor(sensor::Sensor *s) { this->actual_speed_sensor_ = s; }
  void set_role_sensor(sensor::Sensor *s) { this->role_sensor_ = s; }
  void set_temp_offset_sensor(sensor::Sensor *s) { this->temp_offset_sensor_ = s; }
  void set_humidity_offset_sensor(sensor::Sensor *s) { this->humidity_offset_sensor_ = s; }

  // Ecocomfort2Client callbacks
  void on_status() override;
  void on_config() override;
  void on_connect(bool) override {}
  const char *describe() const override { return "Ecocomfort2 Sensor"; }

 protected:
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *voc_sensor_{nullptr};
  sensor::Sensor *direction_sensor_{nullptr};
  sensor::Sensor *actual_mode_sensor_{nullptr};
  sensor::Sensor *actual_speed_sensor_{nullptr};
  sensor::Sensor *role_sensor_{nullptr};
  sensor::Sensor *temp_offset_sensor_{nullptr};
  sensor::Sensor *humidity_offset_sensor_{nullptr};
};

}  // namespace ecocomfort2
}  // namespace esphome

#endif
