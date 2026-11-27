#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/frame_reader.h"
#include "esphome/components/ld24xx/ld24xx.h"
#include "esphome/components/uart/uart.h"

#include <array>

namespace esphome::ld2451 {

// Hi-Link documents no limit; the datasheet example has 3 targets and vendor material claims 5
static constexpr uint8_t MAX_TARGETS = 5;
// Angle, distance, direction, speed, SNR
static constexpr uint8_t TARGET_SIZE = 5;
// Header (4), payload length (2), target count, alarm, targets, footer (4)
static constexpr uint8_t PAYLOAD_OFFSET = ld24xx::HEADER_FOOTER_SIZE + 2;
static constexpr uint8_t MAX_FRAME_LENGTH = PAYLOAD_OFFSET + 2 + MAX_TARGETS * TARGET_SIZE + ld24xx::HEADER_FOOTER_SIZE;

/// Warns about a data frame with more targets than MAX_TARGETS, which is skipped
void warn_too_many_targets(size_t len);
using DataFrame = FrameFormat<ld24xx::DataFrameHeader, ld24xx::PayloadLength,
                              WarnIfLongerThan<MAX_FRAME_LENGTH, warn_too_many_targets>>;

class LD2451Component final : public Component, public uart::UARTDevice {
#ifdef USE_SENSOR
  SUB_SENSOR_WITH_DEDUP(target_count, uint8_t)
#endif

 public:
  void dump_config() override;
  void loop() override;

#ifdef USE_SENSOR
  void set_target_angle_sensor(uint8_t target, sensor::Sensor *s) { this->target_sensors_[target].angle.set_sensor(s); }
  void set_target_distance_sensor(uint8_t target, sensor::Sensor *s) {
    this->target_sensors_[target].distance.set_sensor(s);
  }
  void set_target_speed_sensor(uint8_t target, sensor::Sensor *s) { this->target_sensors_[target].speed.set_sensor(s); }
  void set_target_snr_sensor(uint8_t target, sensor::Sensor *s) { this->target_sensors_[target].snr.set_sensor(s); }
#endif

 protected:
  bool handle_frame_(const uint8_t *frame, size_t len);
  bool handle_data_(const uint8_t *payload, size_t len);

  uint32_t last_rx_{0};
  LengthFrameReader<MAX_FRAME_LENGTH, DataFrame, ld24xx::CmdFrameFormat> reader_;

#ifdef USE_SENSOR
  struct TargetSensors {
    sensor::SensorWithDedup<int8_t> angle;
    sensor::SensorWithDedup<uint8_t> distance;
    sensor::SensorWithDedup<int16_t> speed;
    sensor::SensorWithDedup<uint8_t> snr;
  };
  std::array<TargetSensors, MAX_TARGETS> target_sensors_{};
#endif
};

}  // namespace esphome::ld2451
