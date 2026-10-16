#include "dfrobot_sen0395.h"

#include <cstring>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::dfrobot_sen0395 {

ESPHOME_LOG_TAG(TAG, "dfrobot_sen0395");
const char ASCII_CR = 0x0D;
const char ASCII_LF = 0x0A;

void DfrobotSen0395Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Dfrobot Mmwave Radar:");
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Registered", this->detected_binary_sensor_);
#endif
#ifdef USE_SWITCH
  LOG_SWITCH("  ", "Sensor Active Switch", this->sensor_active_switch_);
  LOG_SWITCH("  ", "Turn on LED Switch", this->turn_on_led_switch_);
  LOG_SWITCH("  ", "Presence via UART Switch", this->presence_via_uart_switch_);
  LOG_SWITCH("  ", "Start after Boot Switch", this->start_after_boot_switch_);
#endif
}

void DfrobotSen0395Component::loop() {
  if (this->cmd_queue_.is_empty()) {
    // Command queue empty. Read sensor state.
    this->cmd_queue_.enqueue({CommandType::COMMAND_TYPE_READ_STATE, 0});
  }

  // Commands are non-blocking and need to be called repeatedly.
  if (this->run_command_(this->cmd_queue_.front())) {
    // Dequeue if command is done
    this->cmd_queue_.dequeue();
    this->cmd_sent_ = false;
    this->retries_left_ = 2;
  }
}

bool DfrobotSen0395Component::enqueue(CommandType type, uint8_t arg) {
  if (type == CommandType::COMMAND_TYPE_SENSITIVITY && arg > 9)
    arg = 9;
  return this->cmd_queue_.enqueue({type, arg});
}

bool DfrobotSen0395Component::run_command_(const Command &cmd) {
  if (cmd.type == CommandType::COMMAND_TYPE_READ_STATE)
    return this->run_read_state_();

  const bool save_cfg = cmd.type == CommandType::COMMAND_TYPE_SAVE_CFG;
  if (!this->cmd_sent_) {
    // The interval between two commands must be larger than the command duration
    if (millis() - this->ts_last_cmd_sent_ > (save_cfg ? 3000 : 1000)) {
      char buf[72];  // "detRangeCfg -1" + 8 * " 32767" + null
      this->format_command_(cmd, buf, sizeof(buf));
      this->write_str(buf);
      this->ts_last_cmd_sent_ = millis();
      this->cmd_sent_ = true;
    }
    return false;  // Command not done yet
  }

  if (this->read_message_()) {
    if (strstr(this->read_buffer_, "is not recognized as a CLI command") != nullptr) {
      ESP_LOGD(TAG, "Command not recognized properly by sensor");
    } else if (this->handle_reply_(cmd)) {
      this->find_prompt_();
      return true;  // Command done
    } else {
      return false;  // Command not done yet
    }
  } else if (millis() - this->ts_last_cmd_sent_ > (save_cfg ? 3500 : 1500)) {
    ESP_LOGD(TAG, "Command timeout");
    if (this->retries_left_ == 0)
      return true;  // Command done
  } else {
    return false;  // Command not done yet
  }

  if (this->retries_left_ == 0) {
    this->find_prompt_();
    return true;  // Command done
  }
  this->retries_left_--;
  this->cmd_sent_ = false;
  ESP_LOGD(TAG, "Retrying");
  return false;
}

bool DfrobotSen0395Component::run_read_state_() {
  if (this->read_message_()) {
    if (strstr(this->read_buffer_, "$JYBSS,0, , , *") != nullptr) {
      this->set_detected_(false);
      this->set_active(true);
      return true;
    }
    if (strstr(this->read_buffer_, "$JYBSS,1, , , *") != nullptr) {
      this->set_detected_(true);
      this->set_active(true);
      return true;
    }
  }
  return millis() - this->ts_last_cmd_sent_ > 500;  // Done on timeout
}

uint8_t DfrobotSen0395Component::read_message_() {
  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);

    if (this->read_pos_ == MMWAVE_READ_BUFFER_LENGTH)
      this->read_pos_ = 0;

    ESP_LOGVV(TAG, "Buffer pos: %u %d", this->read_pos_, byte);

    if (byte == ASCII_CR)
      continue;
    if (byte >= 0x7F)
      byte = '?';  // needs to be valid utf8 string for log functions.
    this->read_buffer_[this->read_pos_] = byte;

    if (this->read_pos_ == 9 && byte == '>')
      this->read_buffer_[++this->read_pos_] = ASCII_LF;

    if (this->read_buffer_[this->read_pos_] == ASCII_LF) {
      this->read_buffer_[this->read_pos_] = 0;
      this->read_pos_ = 0;
      ESP_LOGV(TAG, "Message: %s", this->read_buffer_);
      return 1;  // Full message in buffer
    } else {
      this->read_pos_++;
    }
  }
  return 0;  // No full message yet
}

uint8_t DfrobotSen0395Component::find_prompt_() {
  if (this->read_message_() && strstr(this->read_buffer_, "leapMMW:/>") != nullptr) {
    return 1;  // Prompt found
  }
  return 0;  // Not found yet
}

void DfrobotSen0395Component::set_detected_(bool detected) {
  this->detected_ = detected;
#ifdef USE_BINARY_SENSOR
  if (this->detected_binary_sensor_ != nullptr)
    this->detected_binary_sensor_->publish_state(detected);
#endif
}

bool CircularCommandQueue::enqueue(Command cmd) {
  if (this->is_full()) {
    ESP_LOGE(TAG, "Command queue is full");
    return false;
  }
  if (this->is_empty())
    this->front_ = 0;
  this->rear_ = (this->rear_ + 1) % COMMAND_QUEUE_SIZE;
  this->commands_[this->rear_] = cmd;
  return true;
}

void CircularCommandQueue::dequeue() {
  if (this->is_empty())
    return;
  if (this->front_ == this->rear_) {
    this->front_ = -1;
    this->rear_ = -1;
  } else {
    this->front_ = (this->front_ + 1) % COMMAND_QUEUE_SIZE;
  }
}

}  // namespace esphome::dfrobot_sen0395
