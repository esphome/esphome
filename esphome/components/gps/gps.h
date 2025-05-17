#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include <TinyGPSPlus.h>

#include <vector>

namespace esphome {
namespace gps {

class GPS;

class GPSListener {
 public:
  virtual void on_update(TinyGPSPlus &tiny_gps) = 0;
  TinyGPSPlus &get_tiny_gps();

 protected:
  friend GPS;

  GPS *parent_;
};

class GPS : public PollingComponent, public uart::UARTDevice {
 public:
  void set_latitude_sensor(sensor::Sensor *latitude_sensor) { this->latitude_sensor_ = latitude_sensor; }
  void set_longitude_sensor(sensor::Sensor *longitude_sensor) { this->longitude_sensor_ = longitude_sensor; }
  void set_speed_sensor(sensor::Sensor *speed_sensor) { this->speed_sensor_ = speed_sensor; }
  void set_course_sensor(sensor::Sensor *course_sensor) { this->course_sensor_ = course_sensor; }
  void set_altitude_sensor(sensor::Sensor *altitude_sensor) { this->altitude_sensor_ = altitude_sensor; }
  void set_satellites_sensor(sensor::Sensor *satellites_sensor) { this->satellites_sensor_ = satellites_sensor; }
  void set_hdop_sensor(sensor::Sensor *hdop_sensor) { this->hdop_sensor_ = hdop_sensor; }

  void register_listener(GPSListener *listener) {
    listener->parent_ = this;
    this->listeners_.push_back(listener);
  }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void dump_config() override;
  void loop() override;
  void update() override;

  TinyGPSPlus &get_tiny_gps() { return this->tiny_gps_; }

 protected:
  float latitude_{NAN};
  float longitude_{NAN};
  float speed_{NAN};
  float course_{NAN};
  float altitude_{NAN};
  uint16_t satellites_{0};
  float hdop_{NAN};

  sensor::Sensor *latitude_sensor_{nullptr};
  sensor::Sensor *longitude_sensor_{nullptr};
  sensor::Sensor *speed_sensor_{nullptr};
  sensor::Sensor *course_sensor_{nullptr};
  sensor::Sensor *altitude_sensor_{nullptr};
  sensor::Sensor *satellites_sensor_{nullptr};
  sensor::Sensor *hdop_sensor_{nullptr};

  bool has_time_{false};
  TinyGPSPlus tiny_gps_;
  std::vector<GPSListener *> listeners_{};
};

}  // namespace gps
}  // namespace esphome

#endif  // USE_ARDUINO
