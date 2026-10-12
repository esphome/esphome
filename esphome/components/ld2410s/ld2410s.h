#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif

namespace esphome::ld2410s {

// Largest frame read here is the standard data frame: 4 header + 2 length + payload + 4 footer
static constexpr size_t RX_BUFFER_SIZE = 96;
static_assert(RX_BUFFER_SIZE <= 255, "frame lengths are kept in a byte");

/// The module's common parameters, in the order the parameter read answers them
enum class Parameter : uint8_t {
  MAX_DISTANCE,
  MIN_DISTANCE,
  NO_DELAY,
  STATUS_FREQUENCY,
  DISTANCE_FREQUENCY,
  RESPONSE_SPEED,
  COUNT,
};
static constexpr uint8_t PARAMETER_COUNT = static_cast<uint8_t>(Parameter::COUNT);
// Distances are counted in gates of 0.7 m; the response speed is one of two fixed values
static constexpr float GATE_WIDTH_M = 0.7f;
static constexpr uint32_t RESPONSE_SPEED_NORMAL = 5;
static constexpr uint32_t RESPONSE_SPEED_FAST = 10;

class LD2410SComponent final : public Component, public uart::UARTDevice {
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(target)
#endif
#ifdef USE_SENSOR
  SUB_SENSOR(distance)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(version)
#endif
#ifdef USE_NUMBER
  SUB_NUMBER(max_distance)
  SUB_NUMBER(min_distance)
  SUB_NUMBER(no_delay)
  SUB_NUMBER(status_reporting_frequency)
  SUB_NUMBER(distance_reporting_frequency)
#endif
#ifdef USE_SELECT
  SUB_SELECT(response_speed)
#endif

 public:
  void setup() override { this->status_set_warning(); }  // cleared once the module acknowledged the init sequence
  void loop() override;
  void dump_config() override;

  /// Store a parameter in the module's units and write all of them to the module
  void set_parameter(Parameter parameter, uint32_t value);

 protected:
  enum class FrameType : uint8_t { NONE, SHORT_DATA, STD_DATA, COMMAND };

  void receive_byte_(uint8_t byte);
  /// Returns false when the footer does not match, so the bytes can be scanned again.
  bool handle_frame_();
  /// Returns false for a data frame whose state byte is impossible, so it counts as dropped.
  bool handle_data_frame_(const uint8_t *payload, uint16_t len);
  void handle_command_ack_(const uint8_t *payload, uint16_t len);
  void handle_parameters_(const uint8_t *values, uint16_t len);
  void handle_version_(const uint8_t *values, uint16_t len);
  void count_dropped_frame_() {
    if (this->dropped_frames_ < UINT8_MAX) {
      this->dropped_frames_++;
    }
  }
  void reset_frame_() {
    this->rx_len_ = 0;
    this->expected_len_ = 0;
    this->frame_type_ = FrameType::NONE;
  }
  void start_sequence_(const uint16_t *sequence, uint8_t length, uint32_t now);
  void run_sequence_(uint32_t now);
  uint16_t current_command_() const;
  void send_command_(uint16_t command);
  void publish_target_(bool target, uint16_t distance);
  void publish_parameter_(Parameter parameter);

  const uint16_t *sequence_{nullptr};  // PROGMEM; null while nothing is being sent
  uint32_t next_send_at_{0};
  uint32_t last_frame_at_{0};
  uint32_t parameters_[PARAMETER_COUNT]{};
  uint8_t rx_buffer_[RX_BUFFER_SIZE];
  uint8_t rx_len_{0};
  uint8_t expected_len_{0};
  uint8_t sequence_len_{0};
  uint8_t step_{0};
  uint8_t step_timeouts_{0};
  uint8_t dropped_frames_{0};  // since the last accepted data frame, saturates at 255
  FrameType frame_type_{FrameType::NONE};
  bool awaiting_ack_{false};
  bool rescanning_{false};
  bool init_done_{false};
  bool parameters_dirty_{false};
};

}  // namespace esphome::ld2410s
