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

void Dooya::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Dooya...");
  if (this->header_.empty()) {
    this->header_ = {(uint8_t *)&START_CODE, (uint8_t *)&DEF_ADDR, (uint8_t *)&DEF_ADDR};
  }
}

void Dooya::loop() {
  if ((millis() - this->last_update_) > this->update_interval_) {
    uint8_t data[3] = {READ, this->current_request_, 0x01};
    this->send_command_(data, 3);
    this->last_update_ = millis();
  }
}

void Dooya::on_rs485_data(const std::vector<uint8_t> &data) {
  std::vector<uint8_t> frame(data.begin(), data.end() - 2);
  uint16_t crc = crc16(&frame[0], frame.size());
  if (((crc & 0xFF) == data.end()[-2]) && ((crc >> 8) == data.end()[-1])) {
    switch (data[3]) {
      case CONTROL:
        switch (data[4]) {
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
            if (data[5] > (uint8_t)(this->position * 100))
              this->current_operation = COVER_OPERATION_OPENING;
            else
              this->current_operation = COVER_OPERATION_CLOSING;
            break;
          default:
            ESP_LOGE(TAG, "Invalid control operation received");
            return;
        }
        break;
      case READ:
        switch (this->current_request_) {
          case GET_POSITION:
            this->position = clamp((float) data[5] / 100, 0.0f, 1.0f);
            this->current_request_ = GET_STATUS;
            break;
          case GET_STATUS:
            switch (data[5]) {
              case 0:
                this->current_operation = COVER_OPERATION_IDLE;
                break;
              case 1:
                this->current_operation = COVER_OPERATION_OPENING;
                break;
              case 2:
                this->current_operation = COVER_OPERATION_CLOSING;
                break;
              default:
                ESP_LOGE(TAG, "Invalid status operation received");
                return;
            }
            this->current_request_ = GET_POSITION;
            break;
          default:
            ESP_LOGE(TAG, "Invalid read operation received");
            return;
        }
        break;
      default:
        ESP_LOGE(TAG, "Invalid data type received");
        return;
    }
    if (this->current_operation != this->last_published_op_ || this->position != this->last_published_pos_) {
      this->publish_state(false);
      this->last_published_op_ = this->current_operation;
      this->last_published_pos_ = this->position;
    }
  } else {
    ESP_LOGE(TAG, "Incoming data CRC check failed");
  }
}

void Dooya::send_command_(const uint8_t *data, uint8_t len) {
  std::vector<uint8_t> frame = {START_CODE, *this->header_[1], *this->header_[2]};
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
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X%02X", *this->header_[1], *this->header_[2]);
}

}  // namespace dooya
}  // namespace esphome
