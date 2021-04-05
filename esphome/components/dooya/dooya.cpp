#include "endstop_cover.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dooya {

static const char *TAG = "dooya.cover";

using namespace esphome::cover;

CoverTraits Dooya::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_position(true);
  return traits;
}
void Dooya::control(const CoverCall &call) {
  if (call.get_stop()) {
    uint8_t data[2] = {CONTROL, STOP};
    this->send_command_(data);
  }
  else if (call.get_position().has_value()) {
    auto pos = *call.get_position();
    if (pos != this->position) {
      if (pos == COVER_OPEN) {
        uint8_t data[2] = {CONTROL, OPEN};
        this->send_command_(data);
      }
      else if (pos == COVER_CLOSED) {
        uint8_t data[2] = {CONTROL, CLOSE};
        this->send_command_(data);
      }
      else {
        uint8_t data[3] = {CONTROL, SET_POSITION, (uint8_t)(pos*100)};
        this->send_command_(data);
      }
    }
  }
}

void Dooya::loop() {
  if ((millis() - this->last_status_check_) > 1000) {
    uint8_t data[2] = {READ, this->current_request_};
    this->send_command_(data);
    this->last_status_check_ = millis();
  }

}

void Dooya::on_rs485_data(const std::vector<uint8_t> &data) {
  std::vector<uint8_t> frame(data.begin(), data.end() - 2);
  uint16_t crc = crc16(frame, frame.size());
  if ((((uint8_t) crc >> 8) == data.end()[-2]) && (((uint8_t) crc & 0xFF) == data.end()[-1])) {
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
            if (data[5] > (uint8_t)(this->position*100))
              this->current_operation = COVER_OPERATION_OPENING;
            else
              this->current_operation = COVER_OPERATION_CLOSING;
            break;
          default:
            ESP_LOGE(TAG, "Invalid control operation received")
            return;
        }
        break;
      case READ:
        switch (this->current_request_) {
          case GET_POSITION:
            this->position = (float)std::min(data[5], 100) / 100
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
                ESP_LOGE(TAG, "Invalid status operation received")
                return;
              this->current_request_ = GET_POSITION;
            }
            break;
          default:
            ESP_LOGE(TAG, "Invalid read operation received")
            return;
        }
        break;
      default:
        ESP_LOGE(TAG, "Invalid data type received")
        return;
      this->publish_state();
    }
  } else {
    ESP_LOGE(TAG, "Incoming data CRC check failed")
  }
}

void Dooya::send_command_(uint8_t &data) {
  std::vector<uint8_t> frame = {START_CODE, this->address_[0], this->address_[1]};
  frame.insert(frame.end(), data.begin(), data.end())
  uint16_t crc = crc16(frame, frame.size());
  frame.push_back((uint8_t) crc >> 8);
  frame.push_back((uint8_t) crc & 0xFF);

  this->send(frame);
}

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


}  // namespace dooya
}  // namespace esphome
