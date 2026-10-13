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
#include "esphome/components/ld24xx/ld24xx.h"
#endif
#ifdef USE_LD2410S_VERSION
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#if defined(USE_SELECT) && defined(USE_LD2410S_PARAMETERS)
#include "esphome/components/select/select.h"
#endif

namespace esphome::ld2410s {

// Largest frame read here is the standard data frame: 4 header + 2 length + payload + 4 footer
static constexpr size_t RX_BUFFER_SIZE = 96;
static_assert(RX_BUFFER_SIZE <= 255, "frame lengths are kept in a byte");

/// The module's common parameters, in the order the parameter read answers them
enum class Parameter : uint8_t {
  PARAMETER_MAX_DISTANCE,
  PARAMETER_MIN_DISTANCE,
  PARAMETER_NO_DELAY,
  PARAMETER_STATUS_FREQUENCY,
  PARAMETER_DISTANCE_FREQUENCY,
  PARAMETER_RESPONSE_SPEED,
  PARAMETER_COUNT,
};
static constexpr uint8_t PARAMETER_COUNT = static_cast<uint8_t>(Parameter::PARAMETER_COUNT);
// Response speed values, in the order of the select's options (Normal, Fast)
static constexpr uint8_t RESPONSE_SPEEDS[] PROGMEM = {5, 10};
static constexpr uint8_t RESPONSE_SPEED_COUNT = sizeof(RESPONSE_SPEEDS);

#if defined(USE_NUMBER) && defined(USE_LD2410S_PARAMETERS)
class LD2410SNumber;
#endif

class LD2410SComponent final : public Component, public uart::UARTDevice {
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(target)
#endif
#ifdef USE_SENSOR
  SUB_SENSOR_WITH_DEDUP(distance, uint16_t)
#endif
#ifdef USE_LD2410S_VERSION
  SUB_TEXT_SENSOR(version)
#endif
#if defined(USE_SELECT) && defined(USE_LD2410S_PARAMETERS)
  SUB_SELECT(response_speed)
#endif

 public:
  void setup() override { this->status_set_warning(); }  // cleared once the module acknowledged the init sequence
  void loop() override;
  void dump_config() override;

  /// Store a parameter in the module's units; loop() then writes all of them to the module.
  /// Returns false, and changes nothing, until the module's own values have been read.
#ifdef USE_LD2410S_PARAMETERS
  bool set_parameter(Parameter parameter, uint8_t value);
#ifdef USE_NUMBER
  void set_number(Parameter parameter, LD2410SNumber *number) {
    this->numbers_[static_cast<uint8_t>(parameter)] = number;
  }
#endif
#endif

 protected:
  enum class FrameType : uint8_t { NONE, SHORT_DATA, STD_DATA, COMMAND };

  void receive_byte_(uint8_t byte);
  /// Returns false when the footer does not match, so the bytes can be scanned again.
  bool handle_frame_();
  /// Returns false for a data frame whose state byte is impossible, so it counts as dropped.
  bool handle_data_frame_(const uint8_t *payload, uint16_t len);
  void handle_command_ack_(const uint8_t *payload, uint16_t len);
#ifdef USE_LD2410S_PARAMETERS
  bool handle_parameters_(const uint8_t *values, uint16_t len);
#endif
#ifdef USE_LD2410S_VERSION
  bool handle_version_(const uint8_t *values, uint16_t len);
#endif
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
  void restart_init_(uint32_t at);
  void run_sequence_(uint32_t now);
  uint16_t current_command_() const;
  void send_command_(uint16_t command);
  void publish_target_(bool target, uint16_t distance);
#ifdef USE_LD2410S_PARAMETERS
  void publish_parameters_();
#endif

#if defined(USE_NUMBER) && defined(USE_LD2410S_PARAMETERS)
  LD2410SNumber *numbers_[PARAMETER_COUNT]{};  // null for parameters without a number
#endif

  const uint16_t *sequence_{nullptr};  // PROGMEM; null while nothing is being sent
  uint32_t next_send_at_{0};
  uint32_t last_frame_at_{0};
#ifdef USE_LD2410S_PARAMETERS
  uint8_t parameters_[PARAMETER_COUNT]{};  // all fit a byte: gates, seconds, tenths of a hertz, speed
#endif
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
#ifdef USE_LD2410S_PARAMETERS
  bool parameters_known_{false};  // the module answered the parameter read at least once
  bool parameters_dirty_{false};
#endif
};

}  // namespace esphome::ld2410s
