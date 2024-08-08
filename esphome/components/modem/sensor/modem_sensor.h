#pragma once
#ifdef USE_ESP_IDF

#include "esphome/core/defines.h"

#ifdef USE_MODEM
#ifdef USE_SENSOR

#include "esphome/core/component.h"
#include "esphome/components/modem/modem_component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace modem_sensor {

static const char *const TAG = "modem_sensor";

class ModemSensor : public PollingComponent {
 public:
  void set_rssi_sensor(sensor::Sensor *rssi_sensor) { this->rssi_sensor_ = rssi_sensor; }
  void set_ber_sensor(sensor::Sensor *ber_sensor) { this->ber_sensor_ = ber_sensor; }

  void set_latitude_sensor(sensor::Sensor *latitude_sensor) { this->gnss_latitude_sensor_ = latitude_sensor; }
  void set_longitude_sensor(sensor::Sensor *longitude_sensor) { this->gnss_longitude_sensor_ = longitude_sensor; }
  void set_altitude_sensor(sensor::Sensor *altitude_sensor) { this->gnss_altitude_sensor_ = altitude_sensor; }
  void set_course_sensor(sensor::Sensor *course_sensor) { this->gnss_course_sensor_ = course_sensor; }
  void set_speed_sensor(sensor::Sensor *speed_sensor) { this->gnss_speed_sensor_ = speed_sensor; }
  void set_accuracy_sensor(sensor::Sensor *accuracy_sensor) { this->gnss_accuracy_sensor_ = accuracy_sensor; }

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)

  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void setup() override;
  void update() override;
  void dump_config() override {}

 protected:
  sensor::Sensor *rssi_sensor_{nullptr};
  sensor::Sensor *ber_sensor_{nullptr};
  void update_signal_sensors_();

  sensor::Sensor *gnss_latitude_sensor_{nullptr};
  sensor::Sensor *gnss_longitude_sensor_{nullptr};
  sensor::Sensor *gnss_altitude_sensor_{nullptr};
  sensor::Sensor *gnss_speed_sensor_{nullptr};
  sensor::Sensor *gnss_course_sensor_{nullptr};
  sensor::Sensor *gnss_accuracy_sensor_{nullptr};
  void update_gnss_sensors_();
};

}  // namespace modem_sensor
}  // namespace esphome

#endif  // USE_MODEM
#endif  // USE_SENSOR
#endif  // USE_ESP_IDF
