#include "gm40.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gm40 {

static const char *TAG = "gm40.cover";

using namespace esphome::cover;

uint8_t calc_checksum(std::vector<uint8_t> &frame) {
  uint8_t checksum = 0;
  for (auto i : frame) {
    checksum ^= i;
  }
  return checksum;
}

CoverTraits GM40::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_position(true);
  return traits;
}

void GM40::control(const CoverCall &call) {
  if (call.get_stop()) {
    uint8_t data[5] = {CONTROL, 0x00, STOP, 0x00, 0x01};
    this->send_command_(data, 5);
  } else if (call.get_position().has_value()) {
    this->target_position_ = *call.get_position();
    if (this->target_position_ != this->position) {
      if (this->target_position_ == COVER_OPEN) {
        uint8_t data[5] = {CONTROL, 0x00, OPEN, 0x00, 0x01};
        this->send_command_(data, 5);
      } else if (this->target_position_ == COVER_CLOSED) {
        uint8_t data[5] = {CONTROL, 0x00, CLOSE, 0x00, 0x01};
        this->send_command_(data, 5);
      } else {
        uint8_t data[5] = {CONTROL, 0x00, SET_POSITION, 0x00, (uint8_t)(100 - (this->target_position_ * 100))};
        this->send_command_(data, 5);
      }
    }
  }
}

void GM40::send_update() {
  uint8_t data[5] = {READ, 0x00, SET_POSITION, 0x00, 0x01};
  this->send_command_(data, 5);
}

void GM40::on_uart_multi_byte(uint8_t byte) {
  size_t at = this->rx_buffer_.size();
  switch (at) {
    case 0:
      if (byte == START_CODE_H)
        this->rx_buffer_.push_back(byte);
      break;
    case 1:
      if (byte == START_CODE_L)
        this->rx_buffer_.push_back(byte);
      break;
    case 2:
      if (byte == 0x04 || byte == 0x09)
        this->rx_buffer_.push_back(byte);
      else
        this->rx_buffer_.clear();
      break;
    case 3:
      if (byte == this->address_)
        this->rx_buffer_.push_back(byte);
      else
        this->rx_buffer_.clear();
      break;
    case 4:
      if (byte == CONTROL || byte == READ)
        this->rx_buffer_.push_back(byte);
      else
        this->rx_buffer_.clear();
      break;
    case 5:
    case 10:
      if (byte == 0x00)
        this->rx_buffer_.push_back(byte);
      else
        this->rx_buffer_.clear();
      break;
    case 7:
      this->rx_buffer_.push_back(byte);
      if (this->rx_buffer_[4] == CONTROL) {
        this->process_response_();
        this->rx_buffer_.clear();
      } else if (byte != 0x00)
        this->rx_buffer_.clear();
      break;
    case 8:
      if (byte == 0x01)
        this->rx_buffer_.push_back(byte);
      else
        this->rx_buffer_.clear();
      break;
    case 9:
      if (byte == 0x02)
        this->rx_buffer_.push_back(byte);
      else
        this->rx_buffer_.clear();
      break;
    case 12:
      this->rx_buffer_.push_back(byte);
      this->process_status_();
      this->rx_buffer_.clear();
      break;
    default:
      this->rx_buffer_.push_back(byte);
  }
}

void GM40::process_response_() {
  this->parent_->ready_to_tx = true;
  std::vector<uint8_t> frame(this->rx_buffer_.begin() + 3, this->rx_buffer_.end() - 1);
  if (calc_checksum(frame) == this->rx_buffer_.end()[-1]) {
    switch (this->rx_buffer_[6]) {
      case STOP:
        this->current_operation = COVER_OPERATION_IDLE;
        break;
      case OPEN:
        this->current_operation = COVER_OPERATION_OPENING;
        break;
      case CLOSE:
        this->current_operation = COVER_OPERATION_CLOSING;
        break;
      case SET_POSITION:
        if (this->target_position_ > this->position)
          this->current_operation = COVER_OPERATION_OPENING;
        else
          this->current_operation = COVER_OPERATION_CLOSING;
        break;
      default:
        ESP_LOGE(TAG, "Invalid control operation received");
        return;
    }
    this->publish_state(false);
  } else
    ESP_LOGE(TAG, "Incoming data CRC check failed");
}

void GM40::process_status_() {
  this->parent_->ready_to_tx = true;
  std::vector<uint8_t> frame(this->rx_buffer_.begin() + 3, this->rx_buffer_.end() - 1);
  if (calc_checksum(frame) == this->rx_buffer_.end()[-1]) {
    float pos = clamp((float) (100 - this->rx_buffer_[11]) / 100, 0.0f, 1.0f);
    if (this->position != pos) {
      this->position = pos;
      if (pos == this->target_position_)
        this->current_operation = COVER_OPERATION_IDLE;
      this->publish_state(false);
    }
  } else
    ESP_LOGE(TAG, "Incoming data CRC check failed");
}

void GM40::send_command_(const uint8_t *data, uint8_t len) {
  std::vector<uint8_t> frame = {this->address_};
  for (size_t i = 0; i < len; i++) {
    frame.push_back(data[i]);
  }
  frame.push_back(calc_checksum(frame));
  std::array<uint8_t, 3> header = {START_CODE_H, START_CODE_L, 0x06};
  frame.insert(frame.begin(), header.begin(), header.end());

  this->send(frame);
}

void GM40::dump_config() {
  ESP_LOGCONFIG(TAG, "GM40:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
}

}  // namespace gm40
}  // namespace esphome
