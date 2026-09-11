#include "ld2410s.h"

namespace esphome::ld2410s {

#pragma region LD2410S

void LD2410S::setup() {
  ESP_LOGD(TAG, "setup");

#ifdef LD2410S_V2
  this->publish_distance_(0, true);
  this->publish_presence_(false, true);

  this->publish_calibration_progress_(0, true);
  this->publish_calibration_running_(false, true);

  this->set_threshold_selected_gate(0);

  this->init_();
#endif
}
void LD2410S::loop() {
  if (!this->receive_()) {
    if (!this->pause_tx_) {
      this->send_();
    }
  }
  this->loop_count_++;
}
float LD2410S::get_setup_priority() const { return setup_priority::HARDWARE; }

// prepares scheduled frames for sending
// executes actual data sending
void LD2410S::send_() {
  switch (this->tx_schedule_.check_state(this->loop_count_)) {
    case TxCmdState::SCHEDULED:
      this->build_cmd_frame_(this->tx_schedule_.get_command(), this->tx_schedule_.get_sub_command());
      this->tx_schedule_.confirm_ready_to_send();
      break;

    case TxCmdState::SEND:
      char hex_buf[format_hex_pretty_size(RX_TX_BUFFER_SIZE)];
      ESP_LOGVV(TAG, ">   [loop:%d] cmd:%04x > %s", this->loop_count_, this->tx_schedule_.get_command(),
                format_hex_pretty_to(hex_buf, this->tx_frame_, this->tx_frame_size_, ' '));
      this->write_array(this->tx_frame_, this->tx_frame_size_);
      this->flush();

      this->tx_schedule_.confirm_sent();
      this->sending_pause_();

      break;

    case TxCmdState::FAILED:
      ESP_LOGD(TAG, ">XX [loop:%d] Scheduling command send failed!!!, re-initializing...", this->loop_count_);
      this->tx_schedule_.reset_schedule();
#ifdef LD2410S_V2
      this->pause_tx_ = true;
      this->set_timeout("Pausing Sending", 15000, [this]() {
        this->pause_tx_ = false;
        this->init_();
      });

#endif
      static const uint8_t CFG_END[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xFE, 0x00, 0x04, 0x03, 0x02, 0x01};
      this->write_array(CFG_END, sizeof(CFG_END));
      this->flush();
      break;

    case TxCmdState::IDLE:
      if (!this->init_done_) {
        this->init_done_ = true;
        ESP_LOGV(TAG, "+++ [loop:%d] Setup done", this->loop_count_);
      }
      break;

    case TxCmdState::SENT:
    default:
      break;
  }
}
// builds CMD_FRAME
void LD2410S::build_cmd_frame_(uint16_t command, uint16_t sub_command) {
  ESP_LOGV(TAG, ":>> [loop:%d] cmd:%04x Prepare frame ", this->loop_count_, command);

  this->tx_frame_size_ = 0;

  // Header
  append_seq_data(this->tx_frame_, this->tx_frame_size_, &CMD_FRAME_HEADER);

  // Frame size placeholder
  uint16_t size_start = this->tx_frame_size_;
  this->tx_frame_size_ += sizeof(size_start);

  // Data start
  uint16_t data_start = this->tx_frame_size_;

  // Command
  append_seq_data(this->tx_frame_, this->tx_frame_size_, &command, 1);

  // Parameters
  switch (command) {
    case OUTPUT_MODE_SWITCH_CMD: {
      if (this->minimal_output_) {
        append_seq_data(this->tx_frame_, this->tx_frame_size_, OUTPUT_MODE_VALUE_MIN, 6);
      } else {
        append_seq_data(this->tx_frame_, this->tx_frame_size_, OUTPUT_MODE_VALUE_STD, 6);
      }
    } break;

    case CONFIG_MODE_START_CMD:
      append_seq_data(this->tx_frame_, this->tx_frame_size_, &CONFIG_MODE_START_VALUE);
      break;

    case CONFIG_MODE_END_CMD:
      break;

    case CFG_PARAMS_READ_CMD:

      switch (sub_command) {
        case CFG_MAX_DETECTION_VALUE:
        case CFG_MIN_DETECTION_VALUE:
        case CFG_NO_DELAY_VALUE:
        case CFG_STATUS_FREQ_VALUE:
        case CFG_DISTANCE_FREQ_VALUE:
        case CFG_RESPONSE_SPEED_VALUE:
          append_seq_data(this->tx_frame_, this->tx_frame_size_, &sub_command);
          break;

        default:
          const uint16_t cfg_values[] = {CFG_MAX_DETECTION_VALUE, CFG_MIN_DETECTION_VALUE, CFG_NO_DELAY_VALUE,
                                         CFG_STATUS_FREQ_VALUE,   CFG_DISTANCE_FREQ_VALUE, CFG_RESPONSE_SPEED_VALUE};
          append_seq_data(this->tx_frame_, this->tx_frame_size_, &cfg_values, 6, sizeof(sub_command));
          break;
      }

      break;

    case CFG_FW_READ_CMD:
      break;

    case CFG_PARAMS_WRITE_CMD:
      if (this->resp_speed_ == 0) {
        ESP_LOGD(TAG, "CFG_PARAMS_WRITE_CMD Error, bad new_config");
        return;
      } else {
        switch (sub_command) {
          case CFG_MAX_DETECTION_VALUE:
            append_seq_data_value(this->tx_frame_, this->tx_frame_size_, sub_command, &this->max_dist_);
            break;

          case CFG_MIN_DETECTION_VALUE:
            append_seq_data_value(this->tx_frame_, this->tx_frame_size_, sub_command, &this->min_dist_);
            break;

          case CFG_NO_DELAY_VALUE:
            append_seq_data_value(this->tx_frame_, this->tx_frame_size_, sub_command, &this->delay_);
            break;

          case CFG_STATUS_FREQ_VALUE:
            append_seq_data_value(this->tx_frame_, this->tx_frame_size_, sub_command, &this->status_freq_);
            break;

          case CFG_DISTANCE_FREQ_VALUE:
            append_seq_data_value(this->tx_frame_, this->tx_frame_size_, sub_command, &this->dist_freq_);
            break;

          case CFG_RESPONSE_SPEED_VALUE:
            append_seq_data_value(this->tx_frame_, this->tx_frame_size_, sub_command, &this->resp_speed_);
            break;

          default:
            append_seq_data_value(this->tx_frame_, this->tx_frame_size_, CFG_MAX_DETECTION_VALUE, &this->max_dist_);
            append_seq_data_value(this->tx_frame_, this->tx_frame_size_, CFG_MIN_DETECTION_VALUE, &this->min_dist_);
            append_seq_data_value(this->tx_frame_, this->tx_frame_size_, CFG_NO_DELAY_VALUE, &this->delay_);
            append_seq_data_value(this->tx_frame_, this->tx_frame_size_, CFG_STATUS_FREQ_VALUE, &this->status_freq_);
            append_seq_data_value(this->tx_frame_, this->tx_frame_size_, CFG_DISTANCE_FREQ_VALUE, &this->dist_freq_);
            append_seq_data_value(this->tx_frame_, this->tx_frame_size_, CFG_RESPONSE_SPEED_VALUE, &this->resp_speed_);
            break;
        }
        break;
      }

    case CALIBRATION_CMD:
      append_seq_data(this->tx_frame_, this->tx_frame_size_, &CALIBRATION_TRIGGER_VALUE);
      append_seq_data(this->tx_frame_, this->tx_frame_size_, &CALIBRATION_RETENTION_VALUE);
      append_seq_data(this->tx_frame_, this->tx_frame_size_, &CALIBRATION_TIME_VALUE);
      break;

    case CFG_GATE_THRESHOLD_TRIGGER_READ_CMD:
    case CFG_GATE_THRESHOLD_HOLD_READ_CMD:
    case CFG_GATE_THRESHOLD_SNR_READ_CMD:
      if (sub_command != NO_SUB_CMD) {
        append_seq_data(this->tx_frame_, this->tx_frame_size_, &sub_command);
      } else {
        for (uint16_t i = 0; i < 16; i++) {
          append_seq_data(this->tx_frame_, this->tx_frame_size_, &i);
        }
      }
      break;

    case CFG_GATE_THRESHOLD_TRIGGER_WRITE_CMD:
      append_gate_thresholds(this->tx_frame_, this->tx_frame_size_, sub_command, this->thresholds_trigger_);
      break;

    case CFG_GATE_THRESHOLD_HOLD_WRITE_CMD:
      append_gate_thresholds(this->tx_frame_, this->tx_frame_size_, sub_command, this->thresholds_hold_);
      break;

    case CFG_GATE_THRESHOLD_SNR_WRITE_CMD:
      append_gate_thresholds(this->tx_frame_, this->tx_frame_size_, sub_command, this->thresholds_snr_);
      break;

    default:
      break;
  }

  // Frame size
  uint16_t data_size = this->tx_frame_size_ - data_start;
  append_seq_data(this->tx_frame_, size_start, &data_size);

  // Footer
  append_seq_data(this->tx_frame_, this->tx_frame_size_, &CMD_FRAME_FOOTER);
}

void LD2410S::sending_pause_() {
  this->pause_tx_ = true;
  this->set_timeout("Pausing Sending", TX_PAUSE_TIMEOUT, [this]() {
    ESP_LOGVV("ld2410s", "Proceeding after tx pause of %d ms", TX_PAUSE_TIMEOUT);
    this->pause_tx_ = false;
  });
}

// receives frames and starts processing
bool LD2410S::receive_() {
  uint8_t rx;
  int rx_bytes_count = 0;

  while (this->available() && rx_bytes_count < RX_MAX_BYTES_PER_LOOP) {
    if (!this->read_byte(&rx))
      break;
    rx_bytes_count++;

    if (this->rx_.receive_byte(this->loop_count_, rx) == RxEvaluationResult::OK) {
      this->parse_();
      this->rx_.reset();
    }
  }
  return rx_bytes_count > 0;
}
// starts received frame decoding, and handling received data
void LD2410S::parse_() {
  switch (this->rx_.frame_type()) {
    case RxFrameType::SHORT_DATA_FRAME:
      this->parse_short_data_frame_();
      break;

    case RxFrameType::STD_DATA_FRAME:
      this->parse_data_frame_();
      break;

    case RxFrameType::CMD_FRAME:
      this->parse_cmd_frame_();
      break;

    default:
      char hex_buf[format_hex_pretty_size(RX_TX_BUFFER_SIZE)];
      ESP_LOGD(TAG, "Received Unknown package type!!! < %s",
               format_hex_pretty_to(hex_buf, this->rx_.frame_data(), this->rx_.frame_size(), ' '));
      break;
  }
}
void LD2410S::parse_short_data_frame_() {
  char hex_buf[format_hex_pretty_size(RX_TX_BUFFER_SIZE)];
  ESP_LOGVV(TAG, "<   [loop:%d] short data < %s", this->loop_count_,
            format_hex_pretty_to(hex_buf, this->rx_.frame_data(), this->rx_.frame_size(), ' '));

  if (this->rx_.payload_size() < 3)
    return;

  const bool presence_state = this->rx_.payload_data()[0] > 1;
  uint16_t distance = encode_uint16(this->rx_.payload_data()[2], this->rx_.payload_data()[1]);

  if (!presence_state)
    distance = 0;

#ifdef LD2410S_V2
  this->publish_distance_(distance);
  this->publish_presence_(presence_state);
#endif
}
void LD2410S::parse_data_frame_() {
  if (this->rx_.payload_size() < 1)
    return;

  switch (this->rx_.payload_data()[0]) {
    case 0x01:  // standard data
    {
      if (this->rx_.payload_size() < 4)
        break;

      char hex_buf[format_hex_pretty_size(RX_TX_BUFFER_SIZE)];
      ESP_LOGVV(TAG, "<   [loop:%d] std data < %s", this->loop_count_,
                format_hex_pretty_to(hex_buf, this->rx_.frame_data(), this->rx_.frame_size(), ' '));

      const bool presence_state = this->rx_.payload_data()[1] > 1;

      uint16_t distance = encode_uint16(this->rx_.payload_data()[3], this->rx_.payload_data()[2]);
      if (!presence_state)
        distance = 0;

#ifdef LD2410S_V2
      this->publish_distance_(distance);
      this->publish_presence_(presence_state);

      if (this->rx_.payload_size() >= 7)
        this->parse_data_energy_values_read_(&this->rx_.payload_data()[6]);
#endif

      break;
    }

    case 0x03:  // calibration progress
    {
#ifdef LD2410S_V2
      if (this->rx_.payload_size() < 3)
        break;

      char hex_buf[format_hex_pretty_size(RX_TX_BUFFER_SIZE)];
      ESP_LOGVV(TAG, "<   [loop:%d] std calibration < %s", this->loop_count_,
                format_hex_pretty_to(hex_buf, this->rx_.frame_data(), this->rx_.frame_size(), ' '));

      uint16_t progress = encode_uint16(this->rx_.payload_data()[2], this->rx_.payload_data()[1]);

      this->sending_pause_();

      if (progress == 100) {
        this->publish_calibration_running_(false);
        this->read_all_thresholds_();
      } else {
        this->publish_calibration_running_(true);
      }
      this->publish_calibration_progress_(progress);
#endif

      break;
    }

    default:
      char hex_buf[format_hex_pretty_size(RX_TX_BUFFER_SIZE)];
      ESP_LOGV(TAG, "<XX [loop:%d] std, Unknown std frame type < %s", this->loop_count_,
               format_hex_pretty_to(hex_buf, this->rx_.frame_data(), this->rx_.frame_size(), ' '));
      break;
  }
}
void LD2410S::parse_cmd_frame_() {
  if (this->rx_.payload_size() < 4)
    return;

  uint8_t *data_start = this->rx_.payload_data();
  uint16_t read_position = 0;
  uint16_t command_word = 0;
  uint16_t ack = 0;

  read_seq_data(data_start, read_position, &command_word);
  read_seq_data(data_start, read_position, &ack);

  char hex_buf[format_hex_pretty_size(RX_TX_BUFFER_SIZE)];
  if (ack == 0x0000) {
    ESP_LOGVV(TAG, "<   [loop:%d] cmd:%04x < %s", this->loop_count_, command_word,
              format_hex_pretty_to(hex_buf, this->rx_.frame_data(), this->rx_.frame_size(), ' '));
  } else {
    ESP_LOGD(TAG, "<XX [loop:%d] cmd:%04x Failed ack:%04x < %s", this->loop_count_, command_word, ack,
             format_hex_pretty_to(hex_buf, this->rx_.frame_data(), this->rx_.frame_size(), ' '));
  }

  this->tx_schedule_.verify_response(command_word, this->loop_count_);

  uint8_t *data = &data_start[read_position];

  switch (command_word) {
    // Process acknowledgements

#ifdef LD2410S_V2

    case CONFIG_MODE_START_CMD | CMD_CONFIRMATION:
      if (this->rx_.payload_size() < 8)
        break;
      this->parse_ack_config_start_(data);
      break;

    case CONFIG_MODE_END_CMD | CMD_CONFIRMATION:
      this->parse_ack_config_end_(data);
      break;

    case CALIBRATION_CMD | CMD_CONFIRMATION:
      ESP_LOGI(TAG, "Calibration started");
      break;

      // Write command acknowledgements

    case CFG_PARAMS_WRITE_CMD | CMD_CONFIRMATION:
      ESP_LOGI(TAG, "Config written");
      break;

    case OUTPUT_MODE_SWITCH_CMD | CMD_CONFIRMATION:
      this->parse_ack_minimal_output_(data);
      break;

    case CFG_GATE_THRESHOLD_TRIGGER_WRITE_CMD | CMD_CONFIRMATION:
      ESP_LOGI(TAG, "Trigger Threshold written");
      break;

    case CFG_GATE_THRESHOLD_HOLD_WRITE_CMD | CMD_CONFIRMATION:
      ESP_LOGI(TAG, "Trigger Hold written");
      break;

    case CFG_GATE_THRESHOLD_SNR_WRITE_CMD | CMD_CONFIRMATION:
      ESP_LOGI(TAG, "Trigger SNR written");
      break;

      // Read command acknowledgements

    case CFG_PARAMS_READ_CMD | CMD_CONFIRMATION:
      if (this->rx_.payload_size() < 28)
        break;
      this->parse_ack_config_read_(data);
      break;

    case CFG_FW_READ_CMD | CMD_CONFIRMATION:
      if (this->rx_.payload_size() < 14)
        break;
      this->parse_ack_fw_read_(data);
      break;

    case CFG_GATE_THRESHOLD_TRIGGER_READ_CMD | CMD_CONFIRMATION:
      if (this->rx_.payload_size() < 68)
        break;
      this->parse_ack_threshold_trigger_read_(data);
      break;

    case CFG_GATE_THRESHOLD_HOLD_READ_CMD | CMD_CONFIRMATION:
      if (this->rx_.payload_size() < 68)
        break;
      this->parse_ack_threshold_hold_read_(data);
      break;

    case CFG_GATE_THRESHOLD_SNR_READ_CMD | CMD_CONFIRMATION:
      if (this->rx_.payload_size() < 68)
        break;
      this->parse_ack_threshold_snr_read_(data);
      break;
#endif

    default:
      ESP_LOGD(TAG, "< Unknown: %4x", command_word);
      break;
  }
}

#pragma endregion

#pragma region LD2410Srx

// appends one byte to rx buffer, and checks if that makes complete frame
RxEvaluationResult LD2410Srx::receive_byte(uint32_t loop_count, uint8_t byte) {
  if (this->payload_ready_) {
    this->reset();
  }

  this->rcv_buffer_[this->end_pos_] = byte;

  RxEvaluationResult result = this->evaluate_header_(this->end_pos_ + 1);
  if (result == RxEvaluationResult::OK) {
    result = this->evaluate_size_(this->end_pos_ + 1);
    if (result == RxEvaluationResult::OK) {
      result = this->evaluate_footer_(this->end_pos_ + 1);
    }
  }

  switch (result) {
    case RxEvaluationResult::OK:
      this->payload_ready_ = true;
      break;

    case RxEvaluationResult::UNKNOWN:
      this->end_pos_++;
      if (this->end_pos_ >= RX_TX_BUFFER_SIZE) {
        ESP_LOGV(TAG, "XX< [loop:%d] Received data buffer overflow, resetting", loop_count);
        this->reset();
      }
      break;

    case RxEvaluationResult::NOK:
    default:
      char hex_buf[format_hex_pretty_size(RX_TX_BUFFER_SIZE)];
      ESP_LOGV(TAG, "<XX [loop:%d] %s < %s", loop_count, this->msg_.c_str(),
               format_hex_pretty_to(hex_buf, this->rcv_buffer_, end_pos_ + 1, ' '));
      this->reset();
      result = RxEvaluationResult::UNKNOWN;
      break;
  }

  return result;
}
// checks if current rx buffer contains header
RxEvaluationResult LD2410Srx::evaluate_header_(uint16_t end_pos) {
  switch (this->frame_type_) {
    case RxFrameType::CMD_FRAME:
    case RxFrameType::STD_DATA_FRAME:
    case RxFrameType::SHORT_DATA_FRAME:
      return RxEvaluationResult::OK;  // already determined frame type

    case RxFrameType::NOK:
      return RxEvaluationResult::NOK;  // already determined bad header

    case RxFrameType::UNKNOWN:
    default:
      break;  // need to determine frame type
  }

  if (end_pos == sizeof(SHORT_DATA_FRAME_HEADER) &&
      memcmp(&this->rcv_buffer_[0], &SHORT_DATA_FRAME_HEADER, sizeof(SHORT_DATA_FRAME_HEADER)) == 0) {
    this->frame_type_ = RxFrameType::SHORT_DATA_FRAME;
    this->header_footer_size_ = sizeof(SHORT_DATA_FRAME_HEADER);
    return RxEvaluationResult::OK;
  }

  if (end_pos == sizeof(STD_DATA_FRAME_HEADER) &&
      memcmp(&this->rcv_buffer_[0], &STD_DATA_FRAME_HEADER, sizeof(STD_DATA_FRAME_HEADER)) == 0) {
    this->frame_type_ = RxFrameType::STD_DATA_FRAME;
    this->header_footer_size_ = sizeof(STD_DATA_FRAME_HEADER);
    return RxEvaluationResult::OK;
  }

  if (end_pos == sizeof(CMD_FRAME_HEADER) &&
      memcmp(&this->rcv_buffer_[0], &CMD_FRAME_HEADER, sizeof(CMD_FRAME_HEADER)) == 0) {
    this->frame_type_ = RxFrameType::CMD_FRAME;
    this->header_footer_size_ = sizeof(CMD_FRAME_HEADER);
    return RxEvaluationResult::OK;
  }

  if (end_pos < sizeof(STD_DATA_FRAME_HEADER) && memcmp(&this->rcv_buffer_[0], &STD_DATA_FRAME_HEADER, end_pos) == 0) {
    this->frame_type_ =
        RxFrameType::UNKNOWN;  // not enough data yet to determine frame type, but it fits STD frame header
    this->header_footer_size_ = 0;
    return RxEvaluationResult::UNKNOWN;
  }

  if (end_pos < sizeof(CMD_FRAME_HEADER) && memcmp(&this->rcv_buffer_[0], &CMD_FRAME_HEADER, end_pos) == 0) {
    this->frame_type_ =
        RxFrameType::UNKNOWN;  // not enough data yet to determine frame type, but it fits CMD frame header
    this->header_footer_size_ = 0;
    return RxEvaluationResult::UNKNOWN;
  }

  this->msg_ = "Unknown header";
  this->frame_type_ = RxFrameType::NOK;  // bad header
  return RxEvaluationResult::NOK;
}
// checks if current rx buffer has proper size for decoded header
RxEvaluationResult LD2410Srx::evaluate_size_(uint16_t end_pos) {
  switch (this->frame_type_) {
    case RxFrameType::SHORT_DATA_FRAME:
      if (this->expected_frame_size_ == 0) {
        this->size_field_size_ = 0;
        this->payload_size_ = 3;
        this->payload_pos_ = this->header_footer_size_;
        this->expected_frame_size_ = 2 * this->header_footer_size_ + 3;
      }
      break;

    case RxFrameType::STD_DATA_FRAME:
    case RxFrameType::CMD_FRAME:
      if (this->expected_frame_size_ == 0) {
        this->size_field_size_ = FRAME_DATA_LENGTH_SIZE;
        if (end_pos - 1 >= this->header_footer_size_ + this->size_field_size_) {
          this->payload_size_ = read_int(this->rcv_buffer_, this->header_footer_size_, 2);
          uint16_t overhead = 2 * this->header_footer_size_ + this->size_field_size_;
          if (this->payload_size_ > RX_TX_BUFFER_SIZE - overhead) {
            this->msg_ = "payload size exceeds buffer capacity";
            return RxEvaluationResult::NOK;
          }
          this->payload_pos_ = this->header_footer_size_ + this->size_field_size_;
          this->expected_frame_size_ = overhead + this->payload_size_;
        }
      }
      break;

    case RxFrameType::UNKNOWN:
      return RxEvaluationResult::UNKNOWN;  // not enough data yet to determine size
    case RxFrameType::NOK:                 // already determined bad header
    default:                               // unknown header type
      return RxEvaluationResult::NOK;
  }

  if (this->expected_frame_size_ == 0 || end_pos < this->expected_frame_size_) {
    return RxEvaluationResult::UNKNOWN;  // not enough data yet to determine size

  } else if (end_pos > this->expected_frame_size_) {
    char msg_buf[48];
    snprintf(msg_buf, sizeof(msg_buf), "rx passed the expected frame, expected:%u", this->expected_frame_size_);
    this->msg_ = msg_buf;
    return RxEvaluationResult::NOK;  // passed the end of short data frame

  } else {
    return RxEvaluationResult::OK;  // correct size
  }
}
// checks if current rx buffer containts proper footer for decoded header
RxEvaluationResult LD2410Srx::evaluate_footer_(uint16_t end_pos) {
  switch (this->frame_type_) {
    case RxFrameType::SHORT_DATA_FRAME:  // footer matches expected for short data frame
      if (memcmp(&rcv_buffer_[end_pos - this->header_footer_size_], &SHORT_DATA_FRAME_FOOTER,
                 sizeof(SHORT_DATA_FRAME_FOOTER)) == 0) {
        return RxEvaluationResult::OK;
      }
      break;

    case RxFrameType::STD_DATA_FRAME:  // footer matches expected for standard data frame
      if (memcmp(&rcv_buffer_[end_pos - this->header_footer_size_], &STD_DATA_FRAME_FOOTER,
                 sizeof(STD_DATA_FRAME_FOOTER)) == 0) {
        return RxEvaluationResult::OK;
      }
      break;

    case RxFrameType::CMD_FRAME:  // footer matches expected for command frame
      if (memcmp(&rcv_buffer_[end_pos - this->header_footer_size_], &CMD_FRAME_FOOTER, sizeof(CMD_FRAME_FOOTER)) == 0) {
        return RxEvaluationResult::OK;
      }
      break;

    case RxFrameType::UNKNOWN:  // not enough data yet to determine size
      return RxEvaluationResult::UNKNOWN;
    case RxFrameType::NOK:  // already known bad data frame
    default:                // unknown header type
      break;
  }
  this->msg_ = "footer does not match header: ";
  return RxEvaluationResult::NOK;  // footer does not match expected footer for frame type
}
// reset rx buffer
void LD2410Srx::reset() {
  // ESP_LOGV(TAG, "rx reset");
  std::memset(this->rcv_buffer_, 0, RX_TX_BUFFER_SIZE);
  this->end_pos_ = 0;
  this->header_footer_size_ = 0;
  this->size_field_size_ = 0;
  this->frame_type_ = RxFrameType::UNKNOWN;
  this->payload_ready_ = false;
  this->payload_pos_ = 0;
  this->payload_size_ = 0;
  this->expected_frame_size_ = 0;
}

int LD2410Srx::read_int(const uint8_t *buffer, size_t pos, size_t len) {
  unsigned int ret = 0;
  int shift = 0;
  for (size_t i = 0; i < len; i++) {
    ret |= static_cast<unsigned int>(buffer[pos + i]) << shift;
    shift += 8;
  }
  return ret;
};

#pragma endregion

#pragma region LD2410Sschedule

// Appends new task to schedule
void LD2410Sschedule::append(uint16_t command, uint16_t sub_command) {
  if (this->last_ > 0) {
    ESP_LOGV(TAG, "append => cmd:%04x, prev_cmd:%04x, active:%d, last:%d", command,
             this->commands_[this->last_ - 1].command, this->active_, this->last_);
  } else {
    ESP_LOGV(TAG, "append => cmd:%04x, prev_cmd:none, active:%d, last:%d", command, this->active_, this->last_);
  }

  if (this->last_ >= TX_SCHEDULE_BUFFER_SIZE) {
    ESP_LOGW(TAG, "++: pos:[%d], cmd:%04x, Schedule buffer overflow, resetting buffer !!!", this->last_ - 1, command);

    this->reset_schedule();
    this->state_ = TxCmdState::FAILED;
    return;
  }

  if (command != CONFIG_MODE_START_CMD) {
    if (this->last_ <= 0) {
      ESP_LOGV(TAG, "First cmd must be config start => appending config start and new cmd");
      this->append(CONFIG_MODE_START_CMD);
    } else {
      // if previous cmd is config end, it's not possible to just append new command
      if (this->commands_[this->last_ - 1].command == CONFIG_MODE_END_CMD) {
        if (command == CONFIG_MODE_END_CMD) {
          ESP_LOGV(TAG, "Ignoring duplicated config end cmd");
          return;
        }

        if (this->active_ == this->last_ - 1) {
          ESP_LOGV(TAG, "Previous cmd is config end and it's already executing => appending config start and new cmd");
          this->append(CONFIG_MODE_START_CMD);

        } else {
          ESP_LOGV(TAG, "Last cmd was config end and it's not executing yet => deleting last config end and "
                        "appending new cmd");
          this->last_--;
        }
      }
    }
  }

  ESP_LOGV(TAG, "++: pos:[%d], cmd:%04x", this->last_, command);

  this->commands_[this->last_].command = command;
  this->commands_[this->last_].sub_command = sub_command;

  this->last_++;

  if (command != CONFIG_MODE_START_CMD && command != CONFIG_MODE_END_CMD) {
    ESP_LOGV(TAG, "Appending end");
    this->append(CONFIG_MODE_END_CMD);
  }

  if (this->state_ == TxCmdState::IDLE) {
    this->state_ = TxCmdState::SCHEDULED;
  }
}
// Returns active scheduled task status
TxCmdState LD2410Sschedule::check_state(uint32_t loop_count) {
  switch (this->state_) {
    case TxCmdState::SCHEDULED:
      this->retry_count_ = 0;
      ESP_LOGV(TAG, "::> [loop:%d] pos:%d[%d], cmd:%04x, Scheduled", loop_count, this->active_, this->last_ - 1,
               this->get_command());
      break;

    case TxCmdState::SEND:
      this->time_started_ = App.get_loop_component_start_time();
      ESP_LOGV(TAG, "::> [loop:%d] pos:%d[%d], cmd:%04x, Send", loop_count, this->active_, this->last_ - 1,
               this->get_command());
      break;

    case TxCmdState::SENT:
      if (App.get_loop_component_start_time() > this->time_started_ + TX_CONFIRMATION_TIMEOUT) {
        this->time_started_ = App.get_loop_component_start_time();

        if (this->retry_count_ < TX_MAX_RESEND) {
          ESP_LOGD(TAG, ":>> [loop:%d] pos:%d[%d], cmd:%04x, retry:%d, restart:%d, Send Timeout Expired, Resend!",
                   loop_count, this->active_, this->last_ - 1, this->get_command(), this->retry_count_,
                   this->restart_count_);
          this->state_ = TxCmdState::SEND;
          this->retry_count_++;

        } else {
          if (this->restart_count_ < TX_MAX_RESTART) {
            ESP_LOGW(
                TAG,
                ":>> [loop:%d] pos:%d[:%d], cmd:%04x, retry:%d, restart:%d, Resend limit reached, Restart sequence!!",
                loop_count, this->active_, this->last_ - 1, this->get_command(), this->retry_count_,
                this->restart_count_);
            this->state_ = TxCmdState::SCHEDULED;
            this->retry_count_ = 0;
            this->restart_count_++;
            this->active_ = 0;

          } else {
            ESP_LOGE(
                TAG,
                ":>> [loop:%d] pos:%d[%d], cmd:%04x, retry:%d, restart:%d, Restart sequence limit reached, Giving up, "
                "Reseting buffer!!!",
                loop_count, this->active_, this->last_ - 1, this->get_command(), this->retry_count_,
                this->restart_count_);
            this->state_ = TxCmdState::FAILED;
            this->retry_count_ = 0;
            this->restart_count_ = 0;
            this->active_ = 0;
            this->last_ = 0;
          }
        }
      }
      break;

    case TxCmdState::IDLE:
      // schedule has passed the end
      if (this->last_ > 0 && !this->config_mode_ && this->active_ >= this->last_ - 1) {
        this->reset_schedule();
      }
      break;

    default:
      break;
  }

  return this->state_;
}
// Verifies if received response matches expected, if so procedes to next scheduled command
void LD2410Sschedule::verify_response(uint16_t command_word, uint32_t loop_count) {
  int16_t expected = this->get_command() | CMD_CONFIRMATION;
  if (this->state_ == TxCmdState::SENT) {
    if (command_word == expected) {
      ESP_LOGD(TAG, "Sent cmd: %04x", this->get_command());
      ESP_LOGV(TAG, "::< [loop:%d] pos:%d[%d], cmd:%04x, Sending confirmed, rx:%x", loop_count, this->active_,
               this->last_ - 1, this->get_command(), command_word);

      switch (command_word) {
        // config start confirmed
        case CONFIG_MODE_START_CMD | CMD_CONFIRMATION:
          this->config_mode_ = true;
          break;

        // config end confirmed
        case CONFIG_MODE_END_CMD | CMD_CONFIRMATION:
          this->config_mode_ = false;
          break;

        default:
          break;
      }

      // procede to next task
      this->active_++;
      this->state_ = TxCmdState::SCHEDULED;

      if (this->active_ >= this->last_) {
        ESP_LOGD(TAG, "::: Schedule emptied, Resetting");
        this->reset_schedule();
      }

      if (this->active_ >= TX_SCHEDULE_BUFFER_SIZE) {
        ESP_LOGD(TAG, "::: Schedule overflow, Reseting");
        this->reset_schedule();
      }

    } else {
      if (this->active_ > 0 && command_word == (this->commands_[this->active_ - 1].command | CMD_CONFIRMATION)) {
        ESP_LOGD(
            TAG,
            "::< [loop:%d] pos:%d[%d], cmd:%04x, received:%x, Received unexpected confirmation for previous command",
            loop_count, this->active_, this->last_, this->get_command(), command_word);
      } else {
        ESP_LOGD(TAG, "::< [loop:%d] pos:%d[%d], cmd:%04x, received:%x, Received confirmation for wrong command",
                 loop_count, this->active_, this->last_, this->get_command(), command_word);
      }
    }
  } else {
    ESP_LOGD(TAG, "::< [loop:%d] pos:%d[%d], cmd:%04x, received:%x, Received unexpected command confirmation",
             loop_count, this->active_, this->last_, this->get_command(), command_word);
  }
}

// Confirm frame ready for sending
void LD2410Sschedule::confirm_ready_to_send() { this->state_ = TxCmdState::SEND; }

// Confirm frame sent
void LD2410Sschedule::confirm_sent() {
  this->time_started_ = App.get_loop_component_start_time();
  this->state_ = TxCmdState::SENT;
  this->config_mode_ = true;
}

uint16_t LD2410Sschedule::get_command() { return this->commands_[this->active_].command; }
uint16_t LD2410Sschedule::get_sub_command() { return this->commands_[this->active_].sub_command; }
// Resets schedule buffer
void LD2410Sschedule::reset_schedule() {
  if (this->state_ == TxCmdState::IDLE)
    return;

  std::memset(this->commands_, 0x88, this->last_ * sizeof(TxTaskT));
  this->last_ = 0;
  this->active_ = 0;
  this->time_started_ = App.get_loop_component_start_time();
  this->retry_count_ = 0;
  this->restart_count_ = 0;
  this->state_ = TxCmdState::IDLE;
  ESP_LOGV(TAG, "::: Schedule cleared");
}

#pragma endregion

}  // namespace esphome::ld2410s
