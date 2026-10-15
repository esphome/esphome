#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
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
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#include "esphome/components/uart/uart.h"

#include <array>

namespace esphome::dfrobot_sen0609 {

static constexpr uint8_t MAX_LINE_LENGTH = 80;
static constexpr uint8_t MAX_COMMAND_LENGTH = 40;
static constexpr uint8_t MAX_RESPONSE_VALUES = 3;

enum class RunMode : uint8_t {
  RUN_MODE_PRESENCE = 0,
  RUN_MODE_SPEED_AND_DISTANCE = 1,
};

/// Settings shown as number entities.
enum NumberSetting : uint8_t {
  NUMBER_MIN_RANGE,
  NUMBER_MAX_RANGE,
  NUMBER_TRIGGER_RANGE,
  NUMBER_HOLD_SENSITIVITY,
  NUMBER_TRIGGER_SENSITIVITY,
  NUMBER_ON_LATENCY,
  NUMBER_OFF_LATENCY,
  NUMBER_INHIBIT_TIME,
  NUMBER_THRESHOLD_FACTOR,
  NUMBER_COUNT,
};

/// Settings shown as switch entities.
enum SwitchSetting : uint8_t {
  SWITCH_MICRO_MOTION,
  SWITCH_RUN_LED,
  SWITCH_OUT_LED,
  SWITCH_COUNT,
};

/// Module parameters; a write or read of each is one command. Pending work runs in this order.
enum Param : uint8_t {
  PARAM_RUN_APP,
  PARAM_UART_OUTPUT,
  PARAM_RANGE,
  PARAM_TRIG_RANGE,
  PARAM_SENSITIVITY,
  PARAM_LATENCY,
  PARAM_INHIBIT,
  PARAM_THR_FACTOR,
  PARAM_MICRO_MOTION,
  PARAM_GPIO_MODE,
  PARAM_RUN_LED,
  PARAM_OUT_LED,
  PARAM_SW_VERSION,
  PARAM_HW_VERSION,
  PARAM_COUNT,
};

enum class Operation : uint8_t {
  OPERATION_NONE,
  OPERATION_STOP,
  OPERATION_START,
  OPERATION_WRITE,
  OPERATION_SAVE,
  OPERATION_READ,
};

class DFRobotSEN0609Component final : public Component, public uart::UARTDevice {
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(has_target)
#endif
#ifdef USE_SENSOR
  SUB_SENSOR(target_distance)
  SUB_SENSOR(target_speed)
  SUB_SENSOR(target_energy)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(version)
  SUB_TEXT_SENSOR(hardware_version)
#endif
#ifdef USE_SELECT
  SUB_SELECT(out_pin_level)
#endif

 public:
  explicit DFRobotSEN0609Component(RunMode mode);

  void setup() override;
  void loop() override;
  void dump_config() override;

#ifdef USE_NUMBER
  void set_number(NumberSetting setting, number::Number *number) { this->numbers_[setting] = number; }
  void control_number(NumberSetting setting, float value);
#endif
#ifdef USE_SWITCH
  void set_switch(SwitchSetting setting, switch_::Switch *sw) { this->switches_[setting] = sw; }
  void control_switch(SwitchSetting setting, bool state);
#endif
#ifdef USE_SELECT
  void control_out_pin_level(size_t index);
#endif

 protected:
  void readline_(uint8_t c);
  void handle_line_(char *line);
  void handle_frame_(char *line);
  void log_bad_frame_(const LogString *problem, const char *line);
  void parse_response_(const char *args);

  void queue_configuration_();
  uint16_t wanted_reads_() const;
  void request_write_(Param param);
  void run_sequencer_();
  void check_frames_(uint32_t now);
  bool build_command_(Operation operation, Param param);
  bool apply_read_(Param param);
  void finish_operation_();
  void fail_operation_();
  void hold_(uint32_t duration_ms);
  bool is_aux_operation_() const {
    return this->operation_ == Operation::OPERATION_STOP || this->operation_ == Operation::OPERATION_START;
  }

#ifdef USE_NUMBER
  void publish_number_(NumberSetting setting, float value);
  void forget_number_changes_(Param param, bool unknown);
#endif
#ifdef USE_SWITCH
  void publish_switch_(SwitchSetting setting, bool state);
#endif
#ifdef USE_SENSOR
  void publish_target_(float distance, float speed, float energy);
  Deduplicator<float> distance_dedup_;
  Deduplicator<float> speed_dedup_;
  Deduplicator<float> energy_dedup_;
#endif

#ifdef USE_NUMBER
  std::array<number::Number *, NUMBER_COUNT> numbers_{};
  std::array<float, NUMBER_COUNT> number_values_;
#endif
#ifdef USE_SWITCH
  std::array<switch_::Switch *, SWITCH_COUNT> switches_{};
  std::array<bool, SWITCH_COUNT> switch_states_{};
#endif

  char line_[MAX_LINE_LENGTH];
  char command_[MAX_COMMAND_LENGTH]{};
  float response_[MAX_RESPONSE_VALUES]{};
  uint32_t sent_ms_{0};
  uint32_t hold_start_ms_{0};
  uint32_t hold_ms_{0};
  uint32_t last_frame_ms_{0};
  uint32_t last_change_ms_{0};
  uint32_t deferred_ms_{0};
  uint16_t pending_writes_{0};
  uint16_t pending_reads_{0};
  // Reads made for a write whose paired value is unknown; the write is dropped when the read fails
  uint16_t paired_reads_{0};
  // Writes given up on that are tried again in a later sequence
  uint16_t deferred_writes_{0};
#ifdef USE_NUMBER
  // Numbers changed since their last write was sent, one bit per NumberSetting
  uint16_t changed_numbers_{0};
#endif
  RunMode mode_;
  Operation operation_{Operation::OPERATION_NONE};
  Param param_{PARAM_COUNT};
  uint8_t line_pos_{0};
  uint8_t response_count_{0};
  uint8_t out_pin_level_{0};
  uint8_t attempts_{0};
  uint8_t aux_attempts_{0};
#ifdef USE_SWITCH
  // The module cannot be asked for its LED states: the ones it last accepted, one bit per SwitchSetting
  uint8_t led_known_{0};
  uint8_t led_on_{0};
  bool sent_led_on_{false};
#endif
  bool line_overflow_{false};
  bool read_failed_{false};
  bool bad_frame_logged_{false};
  bool echo_seen_{false};
  bool version_received_{false};
  bool stopped_{false};
  bool save_pending_{false};
  bool deferred_save_{false};
  bool sequence_failed_{false};
};

}  // namespace esphome::dfrobot_sen0609
