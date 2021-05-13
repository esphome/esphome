#include "dooya.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dooya {

static const char *TAG = "dooya.cover";

using namespace esphome::cover;

uint16_t crc16(const uint8_t *data, uint8_t len) {
  uint16_t crc = 0xFFFF;
  while (len--) {
    crc ^= *data++;
    for (uint8_t i = 0; i < 8; i++) {
      if ((crc & 0x01) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

CoverTraits Dooya::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_position(true);
  return traits;
}

void Dooya::control(const CoverCall &call) {
  if (call.get_stop()) {
    uint8_t data[2] = {CONTROL, STOP};
    this->send_command_(data, 2);
  } else if (call.get_position().has_value()) {
    auto pos = *call.get_position();
    if (pos != this->position) {
      if (pos == COVER_OPEN) {
        uint8_t data[2] = {CONTROL, OPEN};
        this->send_command_(data, 2);
      } else if (pos == COVER_CLOSED) {
        uint8_t data[2] = {CONTROL, CLOSE};
        this->send_command_(data, 2);
      } else {
        uint8_t data[3] = {CONTROL, SET_POSITION, (uint8_t)(pos * 100)};
        this->send_command_(data, 3);
      }
    }
  }
}

void Dooya::send_update() {
  uint8_t data[3] = {READ, this->current_request_, 0x01};
  this->send_command_(data, 3);
}

void Dooya::on_uart_multi_byte(uint8_t byte) {
  size_t at = this->rx_buffer_.size();
  switch (at) {
    case 0:
      if (byte == START_CODE)
        this->rx_buffer_.push_back(byte);
      break;
    case 1:
      if (byte == this->address_[0])
        this->rx_buffer_.push_back(byte);
      else
        this->rx_buffer_.clear();
      break;
    case 2:
      if (byte == this->address_[1])
        this->rx_buffer_.push_back(byte);
      else
        this->rx_buffer_.clear();
      break;
    case 3:
      if (byte == CONTROL || byte == READ)
        this->rx_buffer_.push_back(byte);
      else
        this->rx_buffer_.clear();
      break;
    case 6:
      this->rx_buffer_.push_back(byte);
      if (this->rx_buffer_[3] == CONTROL)
        if (this->rx_buffer_[4] != SET_POSITION) {
          this->process_response_();
          this->rx_buffer_.clear();
        }
      break;
    case 7:
      this->rx_buffer_.push_back(byte);
      if (this->rx_buffer_[3] == CONTROL)
        this->process_response_();
      else
        this->process_status_();
      this->rx_buffer_.clear();
      break;
    default:
      this->rx_buffer_.push_back(byte);
  }
}

void Dooya::process_response_() {
  this->parent_->ready_to_tx = true;
  std::vector<uint8_t> frame(this->rx_buffer_.begin(), this->rx_buffer_.end() - 2);
  uint16_t crc = crc16(&frame[0], frame.size());
  if (((crc & 0xFF) == this->rx_buffer_.end()[-2]) && ((crc >> 8) == this->rx_buffer_.end()[-1])) {
    switch (this->rx_buffer_[4]) {
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
        if (this->rx_buffer_[5] > (uint8_t)(this->position * 100))
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

void Dooya::process_status_() {
  this->parent_->ready_to_tx = true;
  std::vector<uint8_t> frame(this->rx_buffer_.begin(), this->rx_buffer_.end() - 2);
  uint16_t crc = crc16(&frame[0], frame.size());
  if (((crc & 0xFF) == this->rx_buffer_.end()[-2]) && ((crc >> 8) == this->rx_buffer_.end()[-1])) {
    if (this->current_request_ == GET_POSITION) {
      float pos = clamp((float) this->rx_buffer_[5] / 100, 0.0f, 1.0f);
      if (this->position != pos) {
        this->position = pos;
        this->publish_state(false);
      }
      this->current_request_ = GET_STATUS;
    } else {
      switch (this->rx_buffer_[5]) {
        case 0:
          if (this->current_operation != COVER_OPERATION_IDLE) {
            this->current_operation = COVER_OPERATION_IDLE;
            this->publish_state(false);
          }
          break;
        case 1:
          if (this->current_operation != COVER_OPERATION_OPENING) {
            this->current_operation = COVER_OPERATION_OPENING;
            this->publish_state(false);
          }
          break;
        case 2:
          if (this->current_operation != COVER_OPERATION_CLOSING) {
            this->current_operation = COVER_OPERATION_CLOSING;
            this->publish_state(false);
          }
          break;
        default:
          ESP_LOGE(TAG, "Invalid status operation received");
          return;
      }
      this->current_request_ = GET_POSITION;
    }
  } else
    ESP_LOGE(TAG, "Incoming data CRC check failed");
}

void Dooya::send_command_(const uint8_t *data, uint8_t len) {
  std::vector<uint8_t> frame = {START_CODE, this->address_[0], this->address_[1]};
  for (size_t i = 0; i < len; i++) {
    frame.push_back(data[i]);
  }
  uint16_t crc = crc16(&frame[0], frame.size());
  frame.push_back(crc >> 0);
  frame.push_back(crc >> 8);

  this->send(frame);
}

void Dooya::dump_config() {
  ESP_LOGCONFIG(TAG, "Dooya:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X%02X", this->address_[0], this->address_[1]);
}

}  // namespace dooya
}  // namespace esphome
