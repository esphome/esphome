#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace distance_uart {

// Enum to represent the operating mode
enum DistanceUARTMode {
  MODE_CONTROLLED,
  MODE_AUTO,
};

// Enum to represent the output mode for AUTO sensors
enum DistanceUARTOutputMode {
  OUTPUT_MODE_PROCESSED,
  OUTPUT_MODE_REALTIME,
};

// Enum to represent the publishing mode
enum DistanceUARTPublishMode {
  PUBLISH_MODE_INTERVAL,
  PUBLISH_MODE_IMMEDIATE,
};

class DistanceUARTSensor : public sensor::Sensor, public PollingComponent, public uart::UARTDevice {
 public:
  // Public methods
  void set_mode(DistanceUARTMode mode);
  void set_trigger_pin(GPIOPin *trigger_pin);
  void set_blind_zone(float blind_zone_m);
  void set_max_range(float max_range_m);
  void set_output_mode(DistanceUARTOutputMode output_mode);
  void set_output_mode_pin(GPIOPin *output_mode_pin);
  void set_publish_mode(DistanceUARTPublishMode publish_mode);
  void set_baud_rate(uint32_t baud_rate);

  void setup() override;
  void dump_config() override;
  void loop() override;
  void update() override;

 protected:
  // Protected methods
  void process_frame_();
  DistanceUARTMode mode_{MODE_AUTO};
  GPIOPin *trigger_pin_{nullptr};
  uint16_t blind_zone_mm_{0};
  uint16_t max_range_mm_{0};
  DistanceUARTOutputMode output_mode_{OUTPUT_MODE_PROCESSED};
  GPIOPin *output_mode_pin_{nullptr};
  DistanceUARTPublishMode publish_mode_{PUBLISH_MODE_INTERVAL};
  uint32_t baud_rate_{9600};
  uint8_t buffer_[4];
  int read_pos_{0};
  float last_distance_m_{NAN};
};

}  // namespace distance_uart
}  // namespace esphome
