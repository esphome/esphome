#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::tx20 {

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
class Tx20Component final : public Component {
  SUB_SENSOR(wind_speed)
  SUB_SENSOR(wind_direction_degrees)

 public:
  /// Get the textual representation of the wind direction ('N', 'SSE', ..).
  std::string get_wind_cardinal_direction() const;

  void set_pin(InternalGPIOPin *pin) { pin_ = pin; }

  void setup() override;
  void dump_config() override;
  void loop() override;

 protected:
  void decode_and_publish_();

  uint8_t wind_direction_index_{0xFF};  // 0xFF until the first reading
  InternalGPIOPin *pin_;
  Tx20ComponentStore store_;
};

}  // namespace esphome::tx20
