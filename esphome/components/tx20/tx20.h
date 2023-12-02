#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace tx20 {

/// Store data in a class that doesn't use multiple-inheritance (vtables in flash)
struct Tx20ComponentStore {
  volatile uint16_t *buffer;
  volatile uint32_t start_time;
  volatile uint8_t buffer_index;
  volatile uint32_t spent_time;
  volatile bool tx20_available;
  volatile bool pin_state;
  ISRInternalGPIOPin pin;

  void reset();
  static void gpio_intr(Tx20ComponentStore *arg);
};

/// This class implements support for the Tx20 Wind sensor.
class Tx20Component : public Component {
 public:
  /// Get the textual representation of the wind direction ('N', 'SSE', ..).
  std::string get_wind_cardinal_direction() const;

  void set_pin(InternalGPIOPin *pin) { pin_ = pin; }
  void set_wind_speed_sensor(sensor::Sensor *wind_speed_sensor) { wind_speed_sensor_ = wind_speed_sensor; }
  void set_wind_direction_degrees_sensor(sensor::Sensor *wind_direction_degrees_sensor) {
    wind_direction_degrees_sensor_ = wind_direction_degrees_sensor;
  }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void loop() override;

 protected:
  void decode_and_publish_();

  std::string wind_cardinal_direction_;
  InternalGPIOPin *pin_;
  sensor::Sensor *wind_speed_sensor_{nullptr};
  sensor::Sensor *wind_direction_degrees_sensor_{nullptr};
  Tx20ComponentStore store_;
};

}  // namespace tx20
}  // namespace esphome
