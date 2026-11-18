#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/frame_reader.h"
#include "esphome/core/hal.h"

namespace esphome::dyp_base {

// Enum to represent the operating mode
enum class DYPMode : uint8_t {
  DYP_MODE_CONTROLLED,
  DYP_MODE_AUTO,
};

// Enum to represent the output mode for AUTO sensors
enum class DYPOutputMode : uint8_t {
  DYP_OUTPUT_MODE_PROCESSED,
  DYP_OUTPUT_MODE_REALTIME,
};

// Enum to represent the publishing mode
enum class DYPPublishMode : uint8_t {
  DYP_PUBLISH_MODE_INTERVAL,
  DYP_PUBLISH_MODE_IMMEDIATE,
};

class DYPSensor : public sensor::Sensor, public PollingComponent, public uart::UARTDevice {
 public:
  void set_mode(DYPMode mode);
  void set_trigger_pin(GPIOPin *trigger_pin);
  void set_blind_zone(float blind_zone_m);
  void set_max_range(float max_range_m);
  void set_output_mode(DYPOutputMode output_mode);
  void set_output_mode_pin(GPIOPin *output_mode_pin);
  void set_publish_mode(DYPPublishMode publish_mode);

  void setup() override;
  void dump_config() override;
  void loop() override;
  void update() override;

 protected:
  void process_frame_(const uint8_t *frame);
  DYPMode mode_{DYPMode::DYP_MODE_AUTO};
  GPIOPin *trigger_pin_{nullptr};
  uint16_t blind_zone_mm_{0};
  uint16_t max_range_mm_{0};
  DYPOutputMode output_mode_{DYPOutputMode::DYP_OUTPUT_MODE_PROCESSED};
  GPIOPin *output_mode_pin_{nullptr};
  DYPPublishMode publish_mode_{DYPPublishMode::DYP_PUBLISH_MODE_INTERVAL};
  FrameReader<4, 0xFF> reader_;
  uint32_t last_rx_time_{0};
  float last_distance_m_{NAN};
};

}  // namespace esphome::dyp_base
