#include "dfrobot_sen0395.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dfrobot_sen0395 {

static const char *const TAG = "dfrobot_sen0395";
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
  if (cmd_queue_.is_empty()) {
    // Command queue empty. Read sensor state.
    cmd_queue_.enqueue(make_unique<ReadStateCommand>());
  }

  // Commands are non-blocking and need to be called repeatedly.
  if (cmd_queue_.process(this)) {
    // Dequeue if command is done
    cmd_queue_.dequeue();
  }
}

int8_t DfrobotSen0395Component::enqueue(std::unique_ptr<Command> cmd) {
  return cmd_queue_.enqueue(std::move(cmd));  // Transfer ownership using std::move
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
  if (this->read_message_()) {
    std::string message(this->read_buffer_);
    if (message.rfind("leapMMW:/>") != std::string::npos) {
      return 1;  // Prompt found
    }
  }
  return 0;  // Not found yet
}

uint8_t DfrobotSen0395Component::send_cmd_(const char *cmd, uint32_t duration) {
  // The interval between two commands must be larger than the specified duration (in ms).
  if (millis() - ts_last_cmd_sent_ > duration) {
    this->write_str(cmd);
    ts_last_cmd_sent_ = millis();
    return 1;  // Command sent
  }
  // Could not send command yet as command duration did not fully pass yet.
  return 0;
}

void DfrobotSen0395Component::set_detected_(bool detected) {
  this->detected_ = detected;
#ifdef USE_BINARY_SENSOR
  if (this->detected_binary_sensor_ != nullptr)
    this->detected_binary_sensor_->publish_state(detected);
#endif
}

int8_t CircularCommandQueue::enqueue(std::unique_ptr<Command> cmd) {
  if (this->is_full()) {
    ESP_LOGE(TAG, "Command queue is full");
    return -1;
  } else if (this->is_empty())
    front_++;
  rear_ = (rear_ + 1) % COMMAND_QUEUE_SIZE;
  commands_[rear_] = std::move(cmd);  // Transfer ownership using std::move
  return 1;
}

std::unique_ptr<Command> CircularCommandQueue::dequeue() {
  if (this->is_empty())
    return nullptr;
  std::unique_ptr<Command> dequeued_cmd = std::move(commands_[front_]);
  if (front_ == rear_) {
    front_ = -1;
    rear_ = -1;
  } else {
    front_ = (front_ + 1) % COMMAND_QUEUE_SIZE;
  }

  return dequeued_cmd;
}

bool CircularCommandQueue::is_empty() { return front_ == -1; }

bool CircularCommandQueue::is_full() { return (rear_ + 1) % COMMAND_QUEUE_SIZE == front_; }

// Run execute method of first in line command.
// Execute is non-blocking and has to be called until it returns 1.
uint8_t CircularCommandQueue::process(DfrobotSen0395Component *parent) {
  if (!is_empty()) {
    return commands_[front_]->execute(parent);
  } else {
    return 1;
  }
}

}  // namespace dfrobot_sen0395
}  // namespace esphome
