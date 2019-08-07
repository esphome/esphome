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

  this->send_message_(&can_message);
}

void Canbus::loop() {
  struct can_frame can_message;
  this->read_message_(&can_message);
  for(auto trigger: this->triggers_){
    if(trigger->can_id_ == can_message.can_id) {
       trigger->trigger();
    }
  }
}

}  // namespace canbus
}  // namespace esphome