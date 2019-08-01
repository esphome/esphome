#include "canbus.h"
#include "esphome/core/log.h"

namespace esphome {
namespace canbus {

static const char *TAG = "canbus";

void Canbus::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Canbus...");
  if (!this->setup_internal_()) {
    ESP_LOGE(TAG, "Canbus setup error!");
    this->mark_failed();
  }
}

void Canbus::dump_config() { ESP_LOGCONFIG(TAG, "Canbus: sender_id=%d", this->sender_id_); }

void Canbus::send_data(uint32_t can_id, const std::vector<uint8_t> data) {
  struct can_frame can_message;

  uint8_t size = static_cast<uint8_t>(data.size());
  ESP_LOGD(TAG, "size=%d", size);
  if (size > CAN_MAX_DLC)
    size = CAN_MAX_DLC;
  can_message.can_dlc = size;
  can_message.can_id = this->sender_id_;

  for (int i = 0; i < size; i++) {
    can_message.data[i] = data[i];
    ESP_LOGD(TAG, "data[%d] = %02x", i, can_message.data[i]);
  }

  // can_message.can_id = this->sender_id_;
  // can_message.can_dlc = 8;
  // can_message.data[0] = 0x00;
  // can_message.data[1] = 0x01;
  // can_message.data[2] = 0x02;
  // can_message.data[3] = 0x03;
  // can_message.data[4] = 0x04;
  // can_message.data[5] = 0x05;
  // can_message.data[6] = 0x06;
  // can_message.data[7] = 0x07;
  //this->dump_frame_(&can_message);

  this->send_message_(&can_message);
}

void Canbus::dump_frame_(const struct can_frame *data_frame) {
  //ESP_LOGD(TAG, "dump_frame");
  //ESP_LOGD(TAG, "canid %d", frame.can_id);
  //ESP_LOGD(TAG, "can_id %02x", data_frame->can_id);
  // for (int i = 0; i < 8; i++) {
  //   data_frame->data[i];
  // }
  return;
}

void Canbus::loop() {
  // check harware inputbuffer and process to esphome outputs
}

}  // namespace canbus
}  // namespace esphome