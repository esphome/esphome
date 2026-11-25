#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/frame_reader.h"
#include "esphome/core/hal.h"

namespace esphome::dyp_base {

enum class DYPMode : uint8_t {
  DYP_MODE_CONTROLLED,
  DYP_MODE_AUTO,
};

// Filter mode of AUTO sensors
enum class DYPFilterMode : uint8_t {
  DYP_FILTER_MODE_PROCESSED,
  DYP_FILTER_MODE_REALTIME,
};

enum class DYPPublishMode : uint8_t {
  DYP_PUBLISH_MODE_INTERVAL,
  DYP_PUBLISH_MODE_IMMEDIATE,
};

class DYPSensor final : public sensor::Sensor, public PollingComponent, public uart::UARTDevice {
 public:
  void set_mode(DYPMode mode) { this->mode_ = mode; }
  void set_trigger_pin(GPIOPin *trigger_pin) { this->trigger_pin_ = trigger_pin; }
  void set_blind_zone_mm(uint16_t blind_zone_mm) { this->blind_zone_mm_ = blind_zone_mm; }
  void set_max_range_mm(uint16_t max_range_mm) { this->max_range_mm_ = max_range_mm; }
  void set_filter_mode(DYPFilterMode filter_mode) { this->filter_mode_ = filter_mode; }
  void set_filter_mode_pin(GPIOPin *filter_mode_pin) { this->filter_mode_pin_ = filter_mode_pin; }
  void set_publish_mode(DYPPublishMode publish_mode) { this->publish_mode_ = publish_mode; }

  void setup() override;
  void dump_config() override;
  void loop() override;
  void update() override;

 protected:
  void process_frame_(const uint8_t *frame);
  GPIOPin *trigger_pin_{nullptr};
  GPIOPin *filter_mode_pin_{nullptr};
  uint32_t last_rx_time_{0};
  float last_distance_m_{NAN};
  uint16_t blind_zone_mm_{0};
  uint16_t max_range_mm_{0};
  FrameReader<4, 0xFF> reader_;
  DYPMode mode_{DYPMode::DYP_MODE_AUTO};
  DYPFilterMode filter_mode_{DYPFilterMode::DYP_FILTER_MODE_PROCESSED};
  DYPPublishMode publish_mode_{DYPPublishMode::DYP_PUBLISH_MODE_INTERVAL};
};

}  // namespace esphome::dyp_base
