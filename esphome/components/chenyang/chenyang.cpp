#include "chenyang.h"
#include "esphome/core/log.h"

namespace esphome {
namespace chenyang {

static const char *TAG = "chenyang.cover";

using namespace esphome::cover;

uint8_t calc_checksum(std::vector<uint8_t> &frame) {
  uint8_t checksum = 0;
  for (auto i : frame) {
    checksum ^= i;
  }
  return checksum;
}

CoverTraits Chenyang::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_position(true);
  return traits;
}

void Chenyang::control(const CoverCall &call) {
  if (call.get_stop()) {
    uint8_t data[2] = {STOP, 0x00};
    this->send_command_(data, 2);
  } else if (call.get_position().has_value()) {
    this->target_position_ = *call.get_position();
    if (this->target_position_ != this->position) {
      if (this->target_position_ == COVER_OPEN) {
        uint8_t data[2] = {OPEN, 0x00};
        this->send_command_(data, 2);
      } else if (this->target_position_ == COVER_CLOSED) {
        uint8_t data[2] = {CLOSE, 0x00};
        this->send_command_(data, 2);
      } else {
        uint8_t data[2] = {SET_POSITION, (uint8_t)(this->target_position_ * 100)};
        this->send_command_(data, 2);
      }
    }
  }
}

void Chenyang::update() {
  uint8_t data[2] = {GET_STATUS, 0x00};
  this->send_command_(data, 2);
}

void Chenyang::on_uart_multi_byte(uint8_t byte) {
  size_t at = this->rx_buffer_.size();
  switch (at) {
    case 0:
      if (byte == RESPONSE || byte == STATUS)
        this->rx_buffer_.push_back(byte);
      break;
    case 1:
      if (byte == this->address_ || byte == 0xFF)
        this->rx_buffer_.push_back(byte);
      else
        this->rx_buffer_.clear();
      break;
    case 4:
      if (this->rx_buffer_[0] == RESPONSE) {
        if (byte == calc_checksum(this->rx_buffer_))
          this->process_response_();
        else
          ESP_LOGE(TAG, "Checksum failed");
        this->rx_buffer_.clear();
      } else
        this->rx_buffer_.push_back(byte);
      break;
    case 7:
    case 12:
      if (byte != 0x78)
        this->rx_buffer_.clear();
      break;
    case 13:
      if (byte == calc_checksum(this->rx_buffer_))
        this->process_status_();
      else
        ESP_LOGE(TAG, "Checksum failed");
      this->rx_buffer_.clear();
      break;
    default:
      this->rx_buffer_.push_back(byte);
  }
}

void Chenyang::process_response_() {
  switch (this->rx_buffer_[2]) {
    case OPEN:
      this->current_operation = COVER_OPERATION_OPENING;
      break;
    case CLOSE:
      this->current_operation = COVER_OPERATION_CLOSING;
      break;
    case STOP:
      this->current_operation = COVER_OPERATION_IDLE;
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
}

void Chenyang::process_status_() {
  bool publish_state = false;
  switch (this->rx_buffer_[2]) {
    case OPEN:
      if (this->current_operation != COVER_OPERATION_OPENING) {
        this->current_operation = COVER_OPERATION_OPENING;
        publish_state = true;
      }
      break;
    case CLOSE:
      if (this->current_operation != COVER_OPERATION_CLOSING) {
        this->current_operation = COVER_OPERATION_CLOSING;
        publish_state = true;
      }
      break;
    case STOP:
      if (this->current_operation != COVER_OPERATION_IDLE) {
        this->current_operation = COVER_OPERATION_IDLE;
        publish_state = true;
      }
      break;
    default:
      ESP_LOGE(TAG, "Invalid status operation received");
      return;
  }
  float pos = clamp((float) this->rx_buffer_[3] / 100, 0.0f, 1.0f);
  if (this->position != pos) {
    this->position = pos;
    publish_state = true;
  }
  if (publish_state)
    this->publish_state(false);
}

void Chenyang::send_command_(const uint8_t *data, uint8_t len) {
  std::vector<uint8_t> frame = {COMMAND, this->address_};
  for (size_t i = 0; i < len; i++) {
    frame.push_back(data[i]);
  }
  frame.push_back(calc_checksum(frame));

  this->send(frame);
}

void Chenyang::dump_config() {
  ESP_LOGCONFIG(TAG, "Chenyang:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
}

}  // namespace chenyang
}  // namespace esphome
