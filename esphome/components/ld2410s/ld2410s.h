#pragma once
#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#include <cstddef>
#include <cstdint>
namespace esphome::ld2410s {
// ld2410s specific Constants
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
static const uint16_t CFG_FW_READ_CMD = 0x0000;
static const uint16_t CFG_PARAMS_READ_CMD = 0x0071;
static const uint16_t CFG_MAX_DETECTION_VALUE = 0x0005;
static const uint16_t CFG_MIN_DETECTION_VALUE = 0x000A;
static const uint16_t CFG_NO_DELAY_VALUE = 0x0006;
static const uint16_t CFG_STATUS_FREQ_VALUE = 0x0002;
static const uint16_t CFG_DISTANCE_FREQ_VALUE = 0x000C;
static const uint16_t CFG_RESPONSE_SPEED_VALUE = 0x000B;
static const uint16_t CFG_GATE_THRESHOLD_TRIGGER_READ_CMD = 0x0073;
static const uint16_t CFG_GATE_THRESHOLD_HOLD_READ_CMD = 0x0077;
static const uint16_t CFG_GATE_THRESHOLD_SNR_READ_CMD = 0x0075;
// Constants
static const uint16_t NO_SUB_CMD = 0xffff;
static const uint16_t FRAME_DATA_LENGTH_SIZE = 2;
static const size_t RX_TX_BUFFER_SIZE = 128;
static const uint16_t RX_MAX_BYTES_PER_LOOP = 128;
static const uint8_t TX_SCHEDULE_BUFFER_SIZE = 32;
static const uint8_t TX_MAX_RESEND = 1;
static const uint8_t TX_MAX_RESTART = 1;
static const uint32_t TX_CONFIRMATION_TIMEOUT = 300;    // timeout for waiting for cmd response
static const uint32_t TX_PAUSE_TIMEOUT = 300;           // pause after receiving response
static const uint32_t TX_REINIT_PAUSE_TIMEOUT = 15000;  // pause before re-running init after a failed schedule
// enum
enum class TxCmdState { IDLE, SCHEDULED, SEND, SENT, FAILED };
enum class RxFrameType { UNKNOWN, SHORT_DATA_FRAME, STD_DATA_FRAME, CMD_FRAME, NOK };
enum class RxEvaluationResult { UNKNOWN, OK, NOK };
// struct
struct TxTaskT {
  uint16_t command;
  uint16_t sub_command;
};
class LD2410Srx {
 public:
  RxEvaluationResult receive_byte(uint32_t loop_count, uint8_t byte);
  RxFrameType frame_type() const { return this->frame_type_; }
  uint8_t *frame_data() { return this->rcv_buffer_; }
  uint16_t frame_size() const { return this->end_pos_; }
  uint8_t *payload_data() { return &this->rcv_buffer_[this->payload_pos_]; }
  uint16_t payload_size() const { return this->payload_size_; }
  bool payload_ready() const { return payload_ready_; }

 protected:
  RxFrameType frame_type_{RxFrameType::UNKNOWN};
  uint16_t end_pos_{0};
  uint16_t header_footer_size_{0};
  uint16_t expected_frame_size_{0};
  uint16_t size_field_size_{0};
  uint16_t payload_pos_{0};
  uint16_t payload_size_{0};
  uint8_t rcv_buffer_[RX_TX_BUFFER_SIZE] = {};
  char msg_[64] = "";
  bool payload_ready_{false};
  RxEvaluationResult evaluate_header_();
  RxEvaluationResult evaluate_size_();
  RxEvaluationResult evaluate_footer_();
  void reset_();
  static int read_int(const uint8_t *buffer, size_t pos, size_t len);
};
class LD2410Sschedule {
 public:
  void append(uint16_t command, uint16_t sub_command = NO_SUB_CMD);
  TxCmdState check_state();
  void confirm_sent();
  void verify_response(uint16_t command_word, uint16_t ack);
  void reset();
  uint16_t get_command();
  uint16_t get_sub_command();

 protected:
  uint32_t time_started_{0};
  TxCmdState state_ = TxCmdState::IDLE;
  uint8_t retry_count_{0};
  uint8_t restart_count_{0};
  uint8_t active_{0};
  uint8_t last_{0};
  bool config_mode_{true};
  TxTaskT commands_[TX_SCHEDULE_BUFFER_SIZE] = {};
  void handle_overflow_(uint16_t command);
};
class LD2410S : public Component, public uart::UARTDevice {
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
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override;

 protected:
  uint32_t thresholds_trigger_[16] = {};
  uint32_t thresholds_hold_[16] = {};
  uint32_t thresholds_snr_[16] = {};
  uint32_t loop_count_{0};
  uint16_t tx_frame_size_ = 0;
  uint8_t tx_frame_[RX_TX_BUFFER_SIZE] = {};
  bool pause_tx_{false};
  bool minimal_output_{true};
  bool init_done_{false};
  LD2410Sschedule tx_schedule_;
  LD2410Srx rx_;
  void init_();
  void read_all_thresholds_();
  void send_();
  bool build_cmd_frame_(uint16_t command, uint16_t sub_command = NO_SUB_CMD);
  void sending_pause_();
  bool receive_();
  void parse_();
  void parse_short_data_frame_();
  void parse_data_frame_();
  void parse_cmd_frame_();
  void publish_distance_(uint16_t distance, bool force_publish = false);
  void publish_presence_(bool presence, bool force_publish = false);
  void publish_calibration_progress_(uint16_t calibration_progress, bool force_publish = false);
  void publish_calibration_running_(bool running, bool force_publish = false);
  void parse_ack_threshold_trigger_read_(uint8_t *data);
  void parse_ack_threshold_hold_read_(uint8_t *data);
  void parse_ack_threshold_snr_read_(uint8_t *data);
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
};
}  // namespace esphome::ld2410s
