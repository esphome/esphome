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
#include "esphome/components/uart/uart.h"

namespace esphome::dfrobot_sen0609 {

static constexpr uint8_t MAX_LINE_LENGTH = 80;
static constexpr uint8_t MAX_COMMAND_LENGTH = 40;

enum class RunMode : uint8_t {
  RUN_MODE_PRESENCE = 0,
  RUN_MODE_SPEED_AND_DISTANCE = 1,
};

/// Module parameters; a write or read of each is one command. Pending work runs in this order.
enum Param : uint8_t {
  PARAM_RUN_APP,
  PARAM_UART_OUTPUT,
  PARAM_SW_VERSION,
  PARAM_HW_VERSION,
  PARAM_COUNT,
};

enum class Operation : uint8_t {
  OPERATION_NONE,
  OPERATION_STOP,
  OPERATION_START,
  OPERATION_WRITE,
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

 public:
  explicit DFRobotSEN0609Component(RunMode mode);

  void setup() override;
  void loop() override;
  void dump_config() override;

 protected:
  void readline_(uint8_t c);
  void handle_line_(char *line);
  void handle_frame_(char *line);

  void queue_configuration_();
  uint16_t wanted_reads_() const;
  void run_sequencer_();
  void build_command_(Operation operation, Param param);
  void finish_operation_();
  void fail_operation_();
  void hold_(uint32_t duration_ms);
  bool is_aux_operation_() const {
    return this->operation_ == Operation::OPERATION_STOP || this->operation_ == Operation::OPERATION_START;
  }

#ifdef USE_SENSOR
  void publish_target_(float distance, float speed, float energy);
  Deduplicator<float> distance_dedup_;
  Deduplicator<float> speed_dedup_;
  Deduplicator<float> energy_dedup_;
#endif

  char line_[MAX_LINE_LENGTH];
  char command_[MAX_COMMAND_LENGTH]{};
  uint32_t sent_ms_{0};
  uint32_t hold_start_ms_{0};
  uint32_t hold_ms_{0};
  uint16_t pending_writes_{0};
  uint16_t pending_reads_{0};
  RunMode mode_;
  Operation operation_{Operation::OPERATION_NONE};
  Param param_{PARAM_COUNT};
  uint8_t line_pos_{0};
  uint8_t attempts_{0};
  uint8_t aux_attempts_{0};
  bool line_overflow_{false};
  bool echo_seen_{false};
  bool stopped_{false};
  bool sequence_failed_{false};
};

}  // namespace esphome::dfrobot_sen0609
