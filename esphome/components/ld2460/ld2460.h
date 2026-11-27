#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/frame_reader.h"
#include "esphome/components/ld24xx/ld24xx.h"
#include "esphome/components/uart/uart.h"

#include <array>

namespace esphome::ld2460 {

static constexpr uint8_t MAX_FRAME_LENGTH = 31;  // A 5-target data frame
static constexpr uint8_t MAX_TARGETS = 5;
// Header (4), command, whole-frame length (2, LE), payload, footer (4)
static constexpr uint8_t MIN_FRAME_LENGTH = 11;
using FrameLength = FrameLengthU16LE<LengthField{.offset = 5}>;
using DataFrame = FrameFormat<ld24xx::DataFrameHeader, FrameLength, FrameMinLength<MIN_FRAME_LENGTH>>;
using CmdFrame = FrameFormat<ld24xx::CmdFrameHeader, FrameLength, FrameMinLength<MIN_FRAME_LENGTH>>;

enum class InstallationMode : uint8_t {
  INSTALLATION_MODE_SIDE = 1,
  INSTALLATION_MODE_TOP = 2,
};

class LD2460Component : public Component, public uart::UARTDevice {
#ifdef USE_SENSOR
  SUB_SENSOR_WITH_DEDUP(target_count, uint8_t)
#endif

 public:
  void setup() override;
  void dump_config() override;
  void loop() override;

#ifdef USE_SENSOR
  void set_target_x_sensor(uint8_t target, sensor::Sensor *s) { this->target_sensors_[target].x.set_sensor(s); }
  void set_target_y_sensor(uint8_t target, sensor::Sensor *s) { this->target_sensors_[target].y.set_sensor(s); }
  void set_target_distance_sensor(uint8_t target, sensor::Sensor *s) {
    this->target_sensors_[target].distance.set_sensor(s);
  }
  void set_target_angle_sensor(uint8_t target, sensor::Sensor *s) { this->target_sensors_[target].angle.set_sensor(s); }
#endif

  /// Add a callback that will be called after each successfully processed periodic data frame.
  template<typename F> void add_on_data_callback(F &&callback) { this->data_callback_.add(std::forward<F>(callback)); }

 protected:
  void send_query_();
  bool handle_periodic_data_(const uint8_t *data, uint8_t packet_len);
  bool handle_ack_(const uint8_t *data, uint8_t len);
  bool handle_frame_(const uint8_t *frame, uint8_t len);

  uint32_t last_rx_{0};
  LengthFrameReader<MAX_FRAME_LENGTH, DataFrame, CmdFrame> reader_;
  // The angle formula depends on how the module is mounted, so it is read from the module at boot
  InstallationMode installation_mode_{InstallationMode::INSTALLATION_MODE_SIDE};
  // Queries sent so far; 0 once the module answered. Data frames resend up to MAX_QUERY_ATTEMPTS.
  uint8_t query_attempts_{1};

#ifdef USE_SENSOR
  // INT16_MIN marks a slot with no published coordinates, so the next report always publishes
  struct TargetSensors {
    sensor::SensorWithDedup<float> x, y, distance, angle;
    int16_t last_x{INT16_MIN};
    int16_t last_y{INT16_MIN};
  };
  std::array<TargetSensors, MAX_TARGETS> target_sensors_{};
#endif

  LazyCallbackManager<void()> data_callback_;
};

}  // namespace esphome::ld2460
