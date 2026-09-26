#pragma once

#define LD2410S_V2

// core
#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

// components
#include "esphome/components/uart/uart.h"

#ifdef LD2410S_V2

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
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

#endif

// std
#include <cstddef>
#include <cstdint>

namespace esphome::ld2410s {

#pragma region ld2410s specific Constants

// Constants
static const char *const TAG = "ld2410s";

static const uint16_t CMD_CONFIRMATION = 0x0100;  // Command confirmation response code

static const uint8_t SHORT_DATA_FRAME_HEADER = 0x6E;
static const uint8_t SHORT_DATA_FRAME_FOOTER = 0x62;
static const uint32_t STD_DATA_FRAME_HEADER = 0xF1F2F3F4;
static const uint32_t STD_DATA_FRAME_FOOTER = 0xF5F6F7F8;
static const uint32_t CMD_FRAME_HEADER = 0xFAFBFCFD;
static const uint32_t CMD_FRAME_FOOTER = 0x01020304;

static const uint16_t CONFIG_MODE_START_CMD = 0x00FF;
static const uint16_t CONFIG_MODE_START_VALUE = 0x0001;
static const uint16_t CONFIG_MODE_END_CMD = 0x00FE;

static const uint16_t OUTPUT_MODE_SWITCH_CMD = 0x007A;
static const uint8_t OUTPUT_MODE_VALUE_STD[] = {0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
static const uint8_t OUTPUT_MODE_VALUE_MIN[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const uint16_t CALIBRATION_CMD = 0x0009;
static const uint16_t CALIBRATION_TRIGGER_VALUE = 0x0002;
static const uint16_t CALIBRATION_RETENTION_VALUE = 0x0001;
static const uint16_t CALIBRATION_TIME_VALUE = 0x0078;

static const uint16_t CFG_FW_READ_CMD = 0x0000;

static const uint16_t CFG_PARAMS_READ_CMD = 0x0071;
static const uint16_t CFG_PARAMS_WRITE_CMD = 0x0070;
static const uint16_t CFG_MAX_DETECTION_VALUE = 0x0005;
static const uint16_t CFG_MIN_DETECTION_VALUE = 0x000A;
static const uint16_t CFG_NO_DELAY_VALUE = 0x0006;
static const uint16_t CFG_STATUS_FREQ_VALUE = 0x0002;
static const uint16_t CFG_DISTANCE_FREQ_VALUE = 0x000C;
static const uint16_t CFG_RESPONSE_SPEED_VALUE = 0x000B;
static const std::string CFG_RESPONSE_SPEED_NORMAL = "Normal";
static const std::string CFG_RESPONSE_SPEED_FAST = "Fast";

static const uint16_t CFG_GATE_THRESHOLD_TRIGGER_READ_CMD = 0x0073;
static const uint16_t CFG_GATE_THRESHOLD_TRIGGER_WRITE_CMD = 0x0072;
static const uint32_t CFG_GATE_THRESHOLD_TRIGGER_WRITE_DATA[] = {
    48, 42, 36, 34, 32, 31, 31, 31, 31,
    31, 31, 31, 31, 31, 31, 31
    // 10~95 dB
};

static const uint16_t CFG_GATE_THRESHOLD_HOLD_READ_CMD = 0x0077;
static const uint16_t CFG_GATE_THRESHOLD_HOLD_WRITE_CMD = 0x0076;
static const uint32_t CFG_GATE_THRESHOLD_HOLD_WRITE_DATA[] = {
    45, 42, 33, 32, 28, 28, 28, 28, 28,
    28, 28, 28, 28, 28, 28, 28
    // 10~95 dB
};

static const uint16_t CFG_GATE_THRESHOLD_SNR_READ_CMD = 0x0075;
static const uint16_t CFG_GATE_THRESHOLD_SNR_WRITE_CMD = 0x0074;
static const uint32_t CFG_GATE_THRESHOLD_SNR_WRITE_DATA[] = {
    51, 50, 30, 28, 25, 25, 25, 25, 25,
    25, 25, 25, 25, 22, 22, 22
    // 5~63 dB
};

// ToDo
// static const uint16_t SN_READ_CMD = 0x0011;
// static const uint16_t SN_WRITE_CMD = 0x0010;

#pragma endregion

#pragma region Constants
static const uint16_t NO_SUB_CMD = 0xffff;
static const uint16_t FRAME_DATA_LENGTH_SIZE = 2;

static const size_t RX_TX_BUFFER_SIZE = 128;
static const uint16_t RX_MAX_BYTES_PER_LOOP = 128;
static const uint8_t TX_SCHEDULE_BUFFER_SIZE = 32;
static const uint8_t TX_MAX_RESEND = 5;
static const uint8_t TX_MAX_RESTART = 5;
static const uint32_t TX_CONFIRMATION_TIMEOUT = 1000;  // timeout for waiting for cmd response
static const uint32_t TX_PAUSE_TIMEOUT = 100;          // pause after receiving response
#pragma endregion

#pragma region enum
enum class TxCmdState { IDLE, SCHEDULED, SEND, SENT, FAILED };
enum class RxFrameType { UNKNOWN, SHORT_DATA_FRAME, STD_DATA_FRAME, CMD_FRAME, NOK };
enum class RxEvaluationResult { UNKNOWN, OK, NOK };
#pragma endregion

#pragma region struct
struct TxTaskT {
  uint16_t command;
  uint16_t sub_command;
};
#pragma endregion

class LD2410Srx {
 public:
  RxEvaluationResult receive_byte(uint32_t loop_count, uint8_t byte);

  RxFrameType frame_type() const { return this->frame_type_; }
  uint8_t *frame_data() { return this->rcv_buffer_; }
  uint8_t frame_size() const { return this->end_pos_; }

  uint8_t *payload_data() { return &this->rcv_buffer_[this->payload_pos_]; }
  uint8_t payload_size() const { return this->payload_size_; }
  bool payload_ready() const { return payload_ready_; }

  void reset();

 protected:
  uint8_t rcv_buffer_[RX_TX_BUFFER_SIZE] = {};
  uint16_t end_pos_{0};

  uint16_t header_footer_size_{0};
  uint16_t expected_frame_size_{0};
  uint16_t size_field_size_{0};

  uint16_t payload_pos_{0};
  uint16_t payload_size_{0};
  RxFrameType frame_type_{RxFrameType::UNKNOWN};
  bool payload_ready_{false};

  std::string msg_{""};

  RxEvaluationResult evaluate_header_(uint16_t end_pos);
  RxEvaluationResult evaluate_size_(uint16_t end_pos);
  RxEvaluationResult evaluate_footer_(uint16_t end_pos);
  static int read_int(const uint8_t *buffer, size_t pos, size_t len);
};

class LD2410Sschedule {
 public:
  void append(uint16_t command, uint16_t sub_command = NO_SUB_CMD);
  TxCmdState check_state(uint32_t loop_count);
  void confirm_ready_to_send();
  void confirm_sent();
  void verify_response(uint16_t command_word, uint32_t loop_count);
  void reset_schedule();
  uint16_t get_command();
  uint16_t get_sub_command();

 protected:
  TxTaskT commands_[TX_SCHEDULE_BUFFER_SIZE] = {};
  uint32_t time_started_{0};
  uint8_t retry_count_{0};
  uint8_t restart_count_{0};
  uint8_t active_{0};
  uint8_t last_{0};
  TxCmdState state_ = TxCmdState::IDLE;
  bool config_mode_{true};
};

class LD2410S : public Component, public uart::UARTDevice {
#ifdef LD2410S_V2
#ifdef USE_SENSOR
  SUB_SENSOR(distance)
#endif
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(presence)
#endif

#ifdef USE_SENSOR
  SUB_SENSOR(calibration_progress)
#endif
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(calibration_running)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(fw_version)
  SUB_TEXT_SENSOR(threshold_trigger)
  SUB_TEXT_SENSOR(threshold_hold)
  SUB_TEXT_SENSOR(threshold_snr)
  SUB_TEXT_SENSOR(energy_values)
#endif
#ifdef USE_BUTTON
  SUB_BUTTON(calibration)
  SUB_BUTTON(factory_reset)
#endif
#ifdef USE_SWITCH
  SUB_SWITCH(minimal_output)
#endif
#ifdef USE_SELECT
  SUB_SELECT(response_speed)
#endif
#ifdef USE_NUMBER
  SUB_NUMBER(max_distance)
  SUB_NUMBER(min_distance)
  SUB_NUMBER(no_delay)
  SUB_NUMBER(status_reporting_freq)
  SUB_NUMBER(distance_reporting_freq)
  SUB_NUMBER(threshold_trigger)
  SUB_NUMBER(threshold_hold)
  SUB_NUMBER(threshold_snr)
  SUB_NUMBER(threshold_selected_gate)
#endif
#endif

 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override;

#ifdef LD2410S_V2
  void dump_config() override;

  // button
  void calibration();
  void factory_reset();
  // number
  void set_delay(float delay);
  void set_distance_reporting_freq(float distance_reporting_freq);
  void set_max_distance(float max_distance);
  void set_min_distance(float min_distance);
  void set_status_reporting_freq(float status_reporting_freq);
  void set_threshold_hold(float threshold_hold);
  void set_threshold_selected_gate(float threshold_selected_gate);
  void set_threshold_snr(float threshold_snr);
  void set_threshold_trigger(float threshold_trigger);
  // select
  void set_response_speed_select(const std::string &response_speed_select);
  // switch
  void set_minimal_output(bool state);
#endif

 protected:
  LD2410Sschedule tx_schedule_;
  LD2410Srx rx_;

  uint8_t tx_frame_[RX_TX_BUFFER_SIZE] = {};
  uint16_t tx_frame_size_ = 0;

  // settings_;
  uint32_t thresholds_trigger_[16] = {};
  uint32_t thresholds_hold_[16] = {};
  uint32_t thresholds_snr_[16] = {};
  uint32_t max_dist_{0};
  uint32_t min_dist_{0};
  uint32_t delay_{0};
  uint32_t status_freq_{0};
  uint32_t dist_freq_{0};
  uint32_t resp_speed_{0};
  uint8_t thresholds_selected_gate_{0};
  bool pause_tx_{false};
  bool minimal_output_{true};

  uint32_t loop_count_{0};
  bool init_done_{false};

  uint32_t energy_values_[16] = {};
  std::string energy_values_str_ = "";

  void send_();
  void build_cmd_frame_(uint16_t command, uint16_t sub_command = NO_SUB_CMD);
  void sending_pause_();

  bool receive_();
  void parse_();
  void parse_short_data_frame_();
  void parse_data_frame_();
  void parse_cmd_frame_();

#ifdef LD2410S_V2
  void init_();
  void read_all_();
  void read_all_thresholds_();

  void parse_data_energy_values_read_(uint8_t *data);

  void parse_ack_config_start_(const uint8_t *data);
  void parse_ack_config_end_(const uint8_t *data);
  void parse_ack_config_read_(uint8_t *data);
  void parse_ack_fw_read_(const uint8_t *data);
  void parse_ack_minimal_output_(uint8_t *data);
  void parse_ack_threshold_trigger_read_(uint8_t *data);
  void parse_ack_threshold_hold_read_(uint8_t *data);
  void parse_ack_threshold_snr_read_(uint8_t *data);

  void publish_distance_(uint16_t distance, bool force_publish = false);
  void publish_presence_(bool presence, bool force_publish = false);
  void publish_calibration_progress_(uint16_t calibration_progress, bool force_publish = false);
  void publish_calibration_running_(bool running, bool force_publish = false);
  void publish_energy_values_(bool force_publish = false);
  void publish_fw_version_(const std::string &version, bool force_publish = false);
  void publish_threshold_trigger_(bool force_publish = false);
  void publish_threshold_hold_(bool force_publish = false);
  void publish_threshold_snr_(bool force_publish = false);

  static std::string format_int(uint32_t *in, uint8_t len, uint8_t min_w);

#endif

  // append variable sized append_data to data, returns true if not overflow
  template<typename T>
  static bool append_seq_data(uint8_t *data, uint16_t &insert_position, const T *append_data,
                              uint16_t append_array_size = 1, uint16_t actual_size = 0) {
    size_t data_object_size = (actual_size == 0 ? sizeof(T) : actual_size);
    auto bytes_to_copy = append_array_size * data_object_size;
    if (insert_position + bytes_to_copy > RX_TX_BUFFER_SIZE) {
      return false;
    }

    auto *write_ptr = &data[0] + insert_position;
    memcpy(write_ptr, append_data, bytes_to_copy);

    insert_position += bytes_to_copy;

    return true;
  }

  // read variable sized uint from data and move read_position
  template<typename T>
  static bool read_seq_data(const uint8_t *data, uint16_t &read_position, T *out_data, uint16_t out_array_size = 1,
                            uint16_t actual_size = 0) {
    size_t data_object_size = (actual_size == 0 ? sizeof(T) : actual_size);
    size_t bytes_to_read = out_array_size * data_object_size;

    if (read_position + bytes_to_read > RX_TX_BUFFER_SIZE) {
      return false;
    }

    const uint8_t *read_ptr = &data[0] + read_position;

    memcpy(out_data, read_ptr, bytes_to_read);

    read_position += bytes_to_read;
    return true;
  }

  template<typename T>
  static bool append_seq_data_value(uint8_t *data, uint16_t &insert_position, uint16_t identifier, const T *append_data,
                                    uint16_t append_array_size = 1, uint16_t actual_size = 0) {
    return append_seq_data(data, insert_position, &identifier) &&
           append_seq_data(data, insert_position, append_data, append_array_size, actual_size);
  }

  static void append_gate_thresholds(uint8_t *data, uint16_t &insert_position, uint16_t sub_command,
                                     const uint32_t *thresholds_array) {
    if (sub_command != NO_SUB_CMD) {
      if (sub_command >= 16)
        return;
      append_seq_data(data, insert_position, &sub_command);
      append_seq_data(data, insert_position, &thresholds_array[sub_command]);
    } else {
      for (uint16_t i = 0; i < 16; i++) {
        append_seq_data(data, insert_position, &i, 1);
        append_seq_data(data, insert_position, &thresholds_array[i]);
      }
    }
  }
};

}  // namespace esphome::ld2410s
