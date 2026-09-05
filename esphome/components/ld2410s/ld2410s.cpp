#include "ld2410s.h"
namespace esphome::ld2410s {
// LD2410S
void LD2410S::setup() {
  ESP_LOGD(TAG, "setup");
  this->publish_distance_(0, true);
  this->publish_presence_(false, true);
  this->publish_calibration_progress_(0, true);
  this->publish_calibration_running_(false, true);
  this->init_();
}
void LD2410S::init_() {
  this->init_done_ = false;
  this->minimal_output_ = true;
  this->tx_schedule_.append(OUTPUT_MODE_SWITCH_CMD);
  this->tx_schedule_.append(CFG_FW_READ_CMD);
  this->tx_schedule_.append(CFG_PARAMS_READ_CMD);
  this->read_all_thresholds_();
}
void LD2410S::read_all_thresholds_() {
  this->tx_schedule_.append(CFG_GATE_THRESHOLD_TRIGGER_READ_CMD);
  this->tx_schedule_.append(CFG_GATE_THRESHOLD_HOLD_READ_CMD);
  this->tx_schedule_.append(CFG_GATE_THRESHOLD_SNR_READ_CMD);
}
void LD2410S::loop() {
  if (!this->init_done_) {
    this->status_set_warning();
  } else {
    this->status_clear_warning();
  }
  if (!this->receive_()) {
    if (!this->pause_tx_) {
      this->send_();
    }
  }
  this->loop_count_++;
}
float LD2410S::get_setup_priority() const { return setup_priority::HARDWARE; }
void LD2410S::send_() {
  switch (this->tx_schedule_.check_state()) {
    case TxCmdState::SCHEDULED:
      if (!this->build_cmd_frame_(this->tx_schedule_.get_command(), this->tx_schedule_.get_sub_command())) {
        this->tx_schedule_.reset();
        break;
      }
      [[fallthrough]];
    case TxCmdState::SEND: {
      this->status_set_warning();
      this->write_array(this->tx_frame_, this->tx_frame_size_);
      this->flush();
      char tx_hex_buf[format_hex_pretty_size(RX_TX_BUFFER_SIZE)];
      ESP_LOGVV(TAG, ">   [%" PRIu32 "] %04x cmd > %s", this->loop_count_, this->tx_schedule_.get_command(),
                format_hex_pretty_to(tx_hex_buf, this->tx_frame_, this->tx_frame_size_, ' '));
      this->init_done_ = false;
      this->tx_schedule_.confirm_sent();
      break;
    }
    case TxCmdState::FAILED:
      this->status_set_warning();
      ESP_LOGD(TAG, ">XX [%" PRIu32 "] Scheduling command send failed, re-initializing", this->loop_count_);
      this->tx_schedule_.reset();
      this->pause_tx_ = true;
      this->set_timeout("Pausing Sending", TX_REINIT_PAUSE_TIMEOUT, [this]() {
        this->pause_tx_ = false;
        this->init_();
      });
      static const uint8_t CFG_END[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xFE, 0x00, 0x04, 0x03, 0x02, 0x01};
      this->write_array(CFG_END, sizeof(CFG_END));
      this->flush();
      break;
    case TxCmdState::IDLE:
      if (!this->init_done_) {
        ESP_LOGV(TAG, "+++ [%" PRIu32 "] Setup done", this->loop_count_);
        this->init_done_ = true;
      }
      break;
    case TxCmdState::SENT:
    default:
      break;
  }
}
bool LD2410S::build_cmd_frame_(uint16_t command, uint16_t sub_command) {
  ESP_LOGV(TAG, ":>> [%" PRIu32 "] %04x Prepare frame ", this->loop_count_, command);
  this->tx_frame_size_ = 0;
  bool ok = append_seq_data(this->tx_frame_, this->tx_frame_size_, &CMD_FRAME_HEADER);
  uint16_t size_start = this->tx_frame_size_;
  this->tx_frame_size_ += sizeof(size_start);
  uint16_t data_start = this->tx_frame_size_;
  ok = ok && append_seq_data(this->tx_frame_, this->tx_frame_size_, &command, 1);
  switch (command) {
    case OUTPUT_MODE_SWITCH_CMD: {
      if (this->minimal_output_) {
        ok = ok && append_seq_data(this->tx_frame_, this->tx_frame_size_, OUTPUT_MODE_VALUE_MIN, 6);
      } else {
        ok = ok && append_seq_data(this->tx_frame_, this->tx_frame_size_, OUTPUT_MODE_VALUE_STD, 6);
      }
    } break;

    case CONFIG_MODE_START_CMD:
      ok = ok && append_seq_data(this->tx_frame_, this->tx_frame_size_, &CONFIG_MODE_START_VALUE);
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
          ok = ok && append_seq_data(this->tx_frame_, this->tx_frame_size_, &sub_command);
          break;
        default:
          const uint16_t cfg_values[] = {CFG_MAX_DETECTION_VALUE, CFG_MIN_DETECTION_VALUE, CFG_NO_DELAY_VALUE,
                                         CFG_STATUS_FREQ_VALUE,   CFG_DISTANCE_FREQ_VALUE, CFG_RESPONSE_SPEED_VALUE};
          ok = ok && append_seq_data(this->tx_frame_, this->tx_frame_size_, &cfg_values, 6, sizeof(sub_command));
          break;
      }
      break;
    case CFG_FW_READ_CMD:
      break;
    case CFG_GATE_THRESHOLD_TRIGGER_READ_CMD:
    case CFG_GATE_THRESHOLD_HOLD_READ_CMD:
    case CFG_GATE_THRESHOLD_SNR_READ_CMD:
      if (sub_command != NO_SUB_CMD) {
        ok = ok && append_seq_data(this->tx_frame_, this->tx_frame_size_, &sub_command);
      } else {
        for (uint16_t i = 0; i < 16; i++) {
          ok = ok && append_seq_data(this->tx_frame_, this->tx_frame_size_, &i);
        }
      }
      break;
    default:
      break;
  }
  if (!ok) {
    ESP_LOGW(TAG, ":XX [%" PRIu32 "] %04x Frame build overflowed tx buffer, dropping", this->loop_count_, command);
    this->tx_frame_size_ = 0;
    return false;
  }
  uint16_t data_size = this->tx_frame_size_ - data_start;
  ok = append_seq_data(this->tx_frame_, size_start, &data_size) &&
       append_seq_data(this->tx_frame_, this->tx_frame_size_, &CMD_FRAME_FOOTER);
  if (!ok) {
    this->tx_frame_size_ = 0;
    return false;
  }
  return true;
}
void LD2410S::sending_pause_() {
  this->pause_tx_ = true;
  this->set_timeout("Pausing Sending", TX_PAUSE_TIMEOUT, [this]() { this->pause_tx_ = false; });
}
bool LD2410S::receive_() {
  uint8_t rx;
  int rx_bytes_count = 0;
  while (this->available() && rx_bytes_count < RX_MAX_BYTES_PER_LOOP) {
    if (!this->read_byte(&rx)) {
      ESP_LOGW(TAG, "UART read failed with bytes still available");
      this->status_set_warning();
      break;
    }
    rx_bytes_count++;
    if (this->rx_.receive_byte(this->loop_count_, rx) == RxEvaluationResult::OK) {
      this->parse_();
    }
  }
  return rx_bytes_count > 0;
}
void LD2410S::parse_() {
  switch (this->rx_.frame_type()) {
    case RxFrameType::SHORT_DATA_FRAME:
      this->parse_short_data_frame_();
      break;
    case RxFrameType::STD_DATA_FRAME:
      this->parse_data_frame_();
      break;
    case RxFrameType::CMD_FRAME:
      this->sending_pause_();
      this->parse_cmd_frame_();
      break;
    default:
      ESP_LOGD(TAG, "Received unknown package type");
      break;
  }
}
void LD2410S::parse_short_data_frame_() {
  char hex_buf[format_hex_pretty_size(RX_TX_BUFFER_SIZE)];
  ESP_LOGVV(TAG, "<   [%" PRIu32 "] short data < %s", this->loop_count_,
            format_hex_pretty_to(hex_buf, this->rx_.frame_data(), this->rx_.frame_size() + 1, ' '));

  const bool presence_state = this->rx_.payload_data()[0] > 1;
  uint16_t distance = encode_uint16(this->rx_.payload_data()[2], this->rx_.payload_data()[1]);
  if (!presence_state)
    distance = 0;
  this->publish_distance_(distance);
  this->publish_presence_(presence_state);
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
      ESP_LOGVV(TAG, "<   [%" PRIu32 "] std data < %s", this->loop_count_,
                format_hex_pretty_to(hex_buf, this->rx_.frame_data(), this->rx_.frame_size() + 1, ' '));

      const bool presence_state = this->rx_.payload_data()[1] > 1;

      uint16_t distance = encode_uint16(this->rx_.payload_data()[3], this->rx_.payload_data()[2]);
      if (!presence_state)
        distance = 0;
      this->publish_distance_(distance);
      this->publish_presence_(presence_state);
      break;
    }
    case 0x03:  // calibration progress
    {
      if (this->rx_.payload_size() < 3)
        break;
      uint16_t progress = encode_uint16(this->rx_.payload_data()[2], this->rx_.payload_data()[1]);
      if (progress == 100) {
        this->publish_calibration_running_(false);
        this->read_all_thresholds_();
      } else {
        this->publish_calibration_running_(true);
      }
      this->publish_calibration_progress_(progress);
      break;
    }

    default:
      char hex_buf[format_hex_pretty_size(RX_TX_BUFFER_SIZE)];
      ESP_LOGV(TAG, "<XX [%" PRIu32 "] std, Unknown std frame type < %s", this->loop_count_,
               format_hex_pretty_to(hex_buf, this->rx_.frame_data(), this->rx_.frame_size() + 1, ' '));

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
    ESP_LOGVV(TAG, "<   [%" PRIu32 "] %04x cmd < %s", this->loop_count_, command_word,
              format_hex_pretty_to(hex_buf, this->rx_.frame_data(), this->rx_.frame_size() + 1, ' '));
  } else {
    ESP_LOGD(TAG, "<XX [%" PRIu32 "] %04x cmd Failed ack:%04x < %s", this->loop_count_, command_word, ack,
             format_hex_pretty_to(hex_buf, this->rx_.frame_data(), this->rx_.frame_size() + 1, ' '));
  }
  this->tx_schedule_.verify_response(command_word, ack);

  uint8_t *data = &data_start[read_position];

  switch (command_word) {
    // Process acknowledgements
    case CFG_GATE_THRESHOLD_TRIGGER_READ_CMD | CMD_CONFIRMATION:
      if (this->rx_.payload_size() >= 68)
        this->parse_ack_threshold_trigger_read_(data);
      break;
    case CFG_GATE_THRESHOLD_HOLD_READ_CMD | CMD_CONFIRMATION:
      if (this->rx_.payload_size() >= 68)
        this->parse_ack_threshold_hold_read_(data);
      break;
    case CFG_GATE_THRESHOLD_SNR_READ_CMD | CMD_CONFIRMATION:
      if (this->rx_.payload_size() >= 68)
        this->parse_ack_threshold_snr_read_(data);
      break;
    default:
      ESP_LOGD(TAG, "< Unknown: %4x", command_word);
      break;
  }
}
void LD2410S::parse_ack_threshold_trigger_read_(uint8_t *data) {
  uint16_t read_position = 0;
  read_seq_data(data, read_position, &this->thresholds_trigger_, 16, 4);
}
void LD2410S::parse_ack_threshold_hold_read_(uint8_t *data) {
  uint16_t read_position = 0;
  read_seq_data(data, read_position, &this->thresholds_hold_, 16, 4);
}
void LD2410S::parse_ack_threshold_snr_read_(uint8_t *data) {
  uint16_t read_position = 0;
  read_seq_data(data, read_position, &this->thresholds_snr_, 16, 4);
}
void LD2410S::publish_distance_(uint16_t distance, bool force_publish) {
#ifdef USE_SENSOR
  if (this->distance_sensor_ != nullptr) {
    if (this->distance_sensor_->state != distance || force_publish) {
      this->distance_sensor_->publish_state(distance);
    }
  }
#endif
}
void LD2410S::publish_presence_(bool presence, bool force_publish) {
#ifdef USE_BINARY_SENSOR
  if (this->presence_binary_sensor_ != nullptr) {
    if (this->presence_binary_sensor_->state != presence || force_publish) {
      this->presence_binary_sensor_->publish_state(presence);
    }
  }
#endif
}
void LD2410S::publish_calibration_progress_(uint16_t calibration_progress, bool force_publish) {
#ifdef USE_SENSOR
  if (this->calibration_progress_sensor_ != nullptr) {
    if (calibration_progress == 100) {
      if (this->calibration_progress_sensor_->state != 0 || force_publish) {
        this->calibration_progress_sensor_->publish_state(0);
      }
    } else {
      if (this->calibration_progress_sensor_->state != calibration_progress || force_publish) {
        this->calibration_progress_sensor_->publish_state(calibration_progress);
      }
    }
  }
#endif
}
void LD2410S::publish_calibration_running_(bool running, bool force_publish) {
#ifdef USE_BINARY_SENSOR
  if (this->calibration_running_binary_sensor_ != nullptr) {
    if (this->calibration_running_binary_sensor_->state != running || force_publish) {
      this->calibration_running_binary_sensor_->publish_state(running);
    }
  }
#endif
}
RxEvaluationResult LD2410Srx::receive_byte(uint32_t loop_count, uint8_t byte) {
  if (this->payload_ready_) {
    this->reset_();
  }

  this->rcv_buffer_[this->end_pos_] = byte;

  RxEvaluationResult result = this->evaluate_header_();
  if (result == RxEvaluationResult::OK) {
    result = this->evaluate_size_();
    if (result == RxEvaluationResult::OK) {
      result = this->evaluate_footer_();
    }
  }

  switch (result) {
    case RxEvaluationResult::OK:
      this->payload_ready_ = true;
      break;

    case RxEvaluationResult::UNKNOWN:
      this->end_pos_++;
      if (this->end_pos_ >= RX_TX_BUFFER_SIZE) {
        ESP_LOGV(TAG, "XX< [%" PRIu32 "] Received data buffer overflow, resetting", loop_count);
        this->reset_();
      }
      break;

    case RxEvaluationResult::NOK:
    default:
      char hex_buffer[format_hex_pretty_size(RX_TX_BUFFER_SIZE)];
      ESP_LOGV(TAG, "<XX [%" PRIu32 "] %s < %s", loop_count, this->msg_,
               format_hex_pretty_to(hex_buffer, this->rcv_buffer_, end_pos_ + 1, ' '));

      this->reset_();
      result = RxEvaluationResult::UNKNOWN;
      break;
  }

  return result;
}
RxEvaluationResult LD2410Srx::evaluate_header_() {
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

  if (this->end_pos_ + 1 == sizeof(SHORT_DATA_FRAME_HEADER) &&
      memcmp(&this->rcv_buffer_[0], &SHORT_DATA_FRAME_HEADER, sizeof(SHORT_DATA_FRAME_HEADER)) == 0) {
    this->frame_type_ = RxFrameType::SHORT_DATA_FRAME;
    this->header_footer_size_ = sizeof(SHORT_DATA_FRAME_HEADER);
    return RxEvaluationResult::OK;
  }

  if (this->end_pos_ + 1 == sizeof(STD_DATA_FRAME_HEADER) &&
      memcmp(&this->rcv_buffer_[0], &STD_DATA_FRAME_HEADER, sizeof(STD_DATA_FRAME_HEADER)) == 0) {
    this->frame_type_ = RxFrameType::STD_DATA_FRAME;
    this->header_footer_size_ = sizeof(STD_DATA_FRAME_HEADER);
    return RxEvaluationResult::OK;
  }

  if (this->end_pos_ + 1 == sizeof(CMD_FRAME_HEADER) &&
      memcmp(&this->rcv_buffer_[0], &CMD_FRAME_HEADER, sizeof(CMD_FRAME_HEADER)) == 0) {
    this->frame_type_ = RxFrameType::CMD_FRAME;
    this->header_footer_size_ = sizeof(CMD_FRAME_HEADER);
    return RxEvaluationResult::OK;
  }

  if (this->end_pos_ + 1 < sizeof(STD_DATA_FRAME_HEADER) &&
      memcmp(&this->rcv_buffer_[0], &STD_DATA_FRAME_HEADER, this->end_pos_ + 1) == 0) {
    this->frame_type_ =
        RxFrameType::UNKNOWN;  // not enough data yet to determine frame type, but it fits STD frame header
    this->header_footer_size_ = 0;
    return RxEvaluationResult::UNKNOWN;
  }

  if (this->end_pos_ + 1 < sizeof(CMD_FRAME_HEADER) &&
      memcmp(&this->rcv_buffer_[0], &CMD_FRAME_HEADER, this->end_pos_ + 1) == 0) {
    this->frame_type_ =
        RxFrameType::UNKNOWN;  // not enough data yet to determine frame type, but it fits CMD frame header
    this->header_footer_size_ = 0;
    return RxEvaluationResult::UNKNOWN;
  }

  snprintf(this->msg_, sizeof(this->msg_), "Unknown header");
  this->frame_type_ = RxFrameType::NOK;  // bad header
  return RxEvaluationResult::NOK;
}
RxEvaluationResult LD2410Srx::evaluate_size_() {
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
        if (this->end_pos_ >= this->header_footer_size_ + this->size_field_size_) {
          this->payload_size_ = read_int(this->rcv_buffer_, this->header_footer_size_, 2);
          this->payload_pos_ = this->header_footer_size_ + this->size_field_size_;
          this->expected_frame_size_ = 2 * this->header_footer_size_ + this->size_field_size_ + this->payload_size_;
        }
      }
      break;

    case RxFrameType::UNKNOWN:
      return RxEvaluationResult::UNKNOWN;  // not enough data yet to determine size
    case RxFrameType::NOK:                 // already determined bad header
    default:                               // unknown header type
      return RxEvaluationResult::NOK;
  }

  if (this->expected_frame_size_ == 0 || this->end_pos_ + 1 < this->expected_frame_size_) {
    return RxEvaluationResult::UNKNOWN;  // not enough data yet to determine size

  } else if (this->end_pos_ + 1 > this->expected_frame_size_) {
    snprintf(this->msg_, sizeof(this->msg_), "rx passed the expected frame, expected:%d", this->expected_frame_size_);

    return RxEvaluationResult::NOK;  // passed the end of short data frame

  } else {
    return RxEvaluationResult::OK;  // correct size
  }
}
RxEvaluationResult LD2410Srx::evaluate_footer_() {
  switch (this->frame_type_) {
    case RxFrameType::SHORT_DATA_FRAME:  // footer matches expected for short data frame
      if (memcmp(&rcv_buffer_[this->end_pos_ - this->header_footer_size_ + 1], &SHORT_DATA_FRAME_FOOTER,
                 sizeof(SHORT_DATA_FRAME_FOOTER)) == 0) {
        return RxEvaluationResult::OK;
      }
      break;

    case RxFrameType::STD_DATA_FRAME:  // footer matches expected for standard data frame
      if (memcmp(&rcv_buffer_[this->end_pos_ - this->header_footer_size_ + 1], &STD_DATA_FRAME_FOOTER,
                 sizeof(STD_DATA_FRAME_FOOTER)) == 0) {
        return RxEvaluationResult::OK;
      }
      break;

    case RxFrameType::CMD_FRAME:  // footer matches expected for command frame
      if (memcmp(&rcv_buffer_[this->end_pos_ - this->header_footer_size_ + 1], &CMD_FRAME_FOOTER,
                 sizeof(CMD_FRAME_FOOTER)) == 0) {
        return RxEvaluationResult::OK;
      }
      break;

    case RxFrameType::UNKNOWN:  // not enough data yet to determine size
      return RxEvaluationResult::UNKNOWN;
    case RxFrameType::NOK:  // already known bad data frame
    default:                // unknown header type
      break;
  }
  snprintf(this->msg_, sizeof(this->msg_), "footer does not match header: ");
  return RxEvaluationResult::NOK;  // footer does not match expected footer for frame type
}
void LD2410Srx::reset_() {
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
// LD2410Sschedule
void LD2410Sschedule::handle_overflow_(uint16_t command) {
  ESP_LOGW(TAG, "++: pos:[%d], cmd:%04x, buffer overflow, resetting buffer", this->last_ - 1, command);
  this->reset();
  this->state_ = TxCmdState::FAILED;
}
void LD2410Sschedule::append(uint16_t command, uint16_t sub_command) {
  uint16_t prev_cmd = (this->last_ > 0) ? this->commands_[this->last_ - 1].command : 0;
  ESP_LOGV(TAG, "append => cmd:%04x, prev_cmd:%04x, active:%d, last:%d", command, prev_cmd, this->active_, this->last_);
  if (this->last_ >= TX_SCHEDULE_BUFFER_SIZE) {
    this->handle_overflow_(command);
    return;
  }
  if (command != CONFIG_MODE_START_CMD) {
    if (this->last_ <= 0) {
      ESP_LOGV(TAG, "First cmd must be config start => appending config start and new cmd");
      this->append(CONFIG_MODE_START_CMD);
    } else {
      // if previous cmd is config end, it's not possible tu just append new command
      if (this->commands_[this->last_ - 1].command == CONFIG_MODE_END_CMD) {
        if (command == CONFIG_MODE_END_CMD) {
          ESP_LOGV(TAG, "Ignoring duplicated config end cmd");
          return;
        }
        if (this->active_ == this->last_ - 1) {
          ESP_LOGV(TAG, "Previous cmd is config end and it's already executing => appending config start and new cmd");
          this->append(CONFIG_MODE_START_CMD);
        } else {
          ESP_LOGV(TAG, "Last cmd was config end and it's not executing executing yet => deleting last config end and "
                        "appending new cmd");
          this->last_--;
        }
      }
    }
  }
  ESP_LOGV(TAG, "++: pos:[%d], cmd:%04x", this->last_, command);
  if (this->last_ >= TX_SCHEDULE_BUFFER_SIZE) {
    // the recursive config-start insert above may have filled the last slot
    this->handle_overflow_(command);
    return;
  }
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

TxCmdState LD2410Sschedule::check_state() {
  switch (this->state_) {
    case TxCmdState::SCHEDULED:
      this->time_started_ = App.get_loop_component_start_time();
      this->retry_count_ = 0;
      ESP_LOGV(TAG, "::> pos:%d[%d], cmd:%04x, Scheduled", this->active_, this->last_ - 1, this->get_command());
      break;

    case TxCmdState::SENT:
      if (App.get_loop_component_start_time() > this->time_started_ + TX_CONFIRMATION_TIMEOUT) {
        this->time_started_ = App.get_loop_component_start_time();

        if (this->retry_count_ < TX_MAX_RESEND) {
          ESP_LOGD(TAG, ":>> pos:%d[%d], cmd:%04x, retry:%d, restart:%d, Send Timeout Expired, Resend!", this->active_,
                   this->last_ - 1, this->get_command(), this->retry_count_, this->restart_count_);
          this->state_ = TxCmdState::SEND;
          this->retry_count_++;

        } else {
          if (this->restart_count_ < TX_MAX_RESTART) {
            ESP_LOGW(TAG, ":>> pos:%d[:%d], cmd:%04x, retry:%d, restart:%d, Resend limit reached, restarting sequence",
                     this->active_, this->last_ - 1, this->get_command(), this->retry_count_, this->restart_count_);
            this->state_ = TxCmdState::SCHEDULED;
            this->retry_count_ = 0;
            this->restart_count_++;
            this->active_ = 0;

          } else {
            ESP_LOGE(TAG,
                     ":>> pos:%d[:%d], cmd:%04x, retry:%d, restart:%d, Restart sequence limit reached, giving up, "
                     "resetting buffer",
                     this->active_, this->last_ - 1, this->get_command(), this->retry_count_, this->restart_count_);
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
        this->reset();
      }

      break;

    case TxCmdState::SEND:
    default:
      break;
  }

  return this->state_;
}
void LD2410Sschedule::verify_response(uint16_t command_word, uint16_t ack) {
  uint16_t expected = this->get_command() | CMD_CONFIRMATION;
  if (command_word == expected) {
    if (ack != 0x0000) {
      // sensor rejected the command; leave state_ as SENT so the SENT-timeout retry in check_state() re-sends it
      ESP_LOGW(TAG, "::< pos:%d[%d], cmd:%04x, Sensor rejected command, ack:%04x", this->active_, this->last_ - 1,
               this->get_command(), ack);
      return;
    }
    ESP_LOGD(TAG, "Sent cmd: %04x", this->get_command());
    ESP_LOGV(TAG, "::< pos:%d[%d], cmd:%04x, Sending confirmed, rx:%x", this->active_, this->last_ - 1,
             this->get_command(), command_word);

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

    if (this->active_ >= this->last_ - 1 && this->last_ > 0 && !this->config_mode_) {
      this->reset();
      return;
    }

    // procede to next task
    this->state_ = TxCmdState::SCHEDULED;
    this->active_++;
    if (this->active_ >= TX_SCHEDULE_BUFFER_SIZE) {
      ESP_LOGD(TAG, "::: Schedule overflow, resetting");
      this->reset();
    }

  } else {
    // desync between what we sent and what the sensor confirmed -- escalate above debug so it's not invisible
    if (this->state_ == TxCmdState::SENT) {
      ESP_LOGW(TAG, "::< pos:%d[%d], cmd:%04x, received:%x, received confirmation for wrong command", this->active_,
               this->last_, this->get_command(), command_word);
    } else {
      if (this->active_ > 0 && command_word == (this->commands_[this->active_ - 1].command | CMD_CONFIRMATION)) {
        ESP_LOGW(TAG, "::< pos:%d[%d], cmd:%04x, received:%x, received unexpected confirmation for previous cmd",
                 this->active_, this->last_, this->get_command(), command_word);
      } else {
        ESP_LOGW(TAG, "::< pos:%d[%d], cmd:%04x, received:%x, received unexpected confirmation", this->active_,
                 this->last_, this->get_command(), command_word);
      }
    }
  }
}
void LD2410Sschedule::confirm_sent() {
  this->time_started_ = App.get_loop_component_start_time();
  this->state_ = TxCmdState::SENT;
  this->config_mode_ = true;
}
uint16_t LD2410Sschedule::get_command() { return this->commands_[this->active_].command; }
uint16_t LD2410Sschedule::get_sub_command() { return this->commands_[this->active_].sub_command; }
void LD2410Sschedule::reset() {
  this->last_ = 0;
  this->active_ = 0;
  this->time_started_ = App.get_loop_component_start_time();
  this->retry_count_ = 0;
  this->restart_count_ = 0;
  this->state_ = TxCmdState::IDLE;
  ESP_LOGV(TAG, "::: Schedule cleared");
}
}  // namespace esphome::ld2410s
