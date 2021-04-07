#include "chenyang.h"
#include "esphome/core/log.h"

namespace esphome {
namespace chenyang {

static const char *TAG = "chenyang.cover";

using namespace esphome::cover;

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
    auto pos = *call.get_position();
    if (pos != this->position) {
      if (pos == COVER_OPEN) {
        uint8_t data[2] = {OPEN, 0x00};
        this->send_command_(data, 2);
      } else if (pos == COVER_CLOSED) {
        uint8_t data[2] = {CLOSE, 0x00};
        this->send_command_(data, 2);
      } else {
        uint8_t data[2] = {SET_POSITION, (uint8_t)(pos * 100)};
        this->send_command_(data, 2);
      }
    }
  }
}

void Chenyang::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Chenyang...");
  if (this->header_.empty())
    this->header_ = {nullptr, (uint8_t *) &DEF_ADDR};
}

void Chenyang::loop() {
  if ((millis() - this->last_update_) > this->update_interval_) {
    uint8_t data[2] = {GET_STATUS, 0x00};
    this->send_command_(data, 2);
    this->last_update_ = millis();
  }
}

void Chenyang::on_rs485_data(const std::vector<uint8_t> &data) {
  std::vector<uint8_t> frame(data.begin(), data.end() - 1);
  uint8_t checksum = 0;
  for (auto i : frame) {
    checksum ^= i;
  }
  if (checksum == data.end()[-1]) {
    switch (data[0]) {
      case RESPONSE:
        switch (data[2]) {
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
            if (data[3] > (uint8_t)(this->position * 100))
              this->current_operation = COVER_OPERATION_OPENING;
            else
              this->current_operation = COVER_OPERATION_CLOSING;
            break;
          default:
            ESP_LOGE(TAG, "Invalid control operation received");
            return;
        }
        break;
      case STATUS:
        switch (data[2]) {
          case OPEN:
            this->current_operation = COVER_OPERATION_OPENING;
            break;
          case CLOSE:
            this->current_operation = COVER_OPERATION_CLOSING;
            break;
          case STOP:
            this->current_operation = COVER_OPERATION_IDLE;
            break;
          default:
            ESP_LOGE(TAG, "Invalid status operation received");
            return;
        }
        this->position = clamp((float) data[3] / 100, 0.0f, 1.0f);
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
    ESP_LOGE(TAG, "Incoming data checksum failed");
  }
}

void Chenyang::send_command_(const uint8_t *data, uint8_t len) {
  std::vector<uint8_t> frame = {COMMAND, *this->header_[1]};
  for (size_t i = 0; i < len; i++) {
    frame.push_back(data[i]);
  }
  uint8_t checksum;
  for (auto i : frame) {
    checksum ^= i;
  }
  frame.push_back(checksum);

  this->send(frame);
}

void Chenyang::dump_config() {
  ESP_LOGCONFIG(TAG, "Chenyang:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", *this->header_[1]);
}

}  // namespace chenyang
}  // namespace esphome
