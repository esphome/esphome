#include "canbus.h"
#include "esphome/core/log.h"

namespace esphome {
namespace canbus {

static const char *TAG = "canbus";

void Canbus::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Canbus...");
  if (!this->setup_internal()) {
    ESP_LOGE(TAG, "Canbus setup error!");
    this->mark_failed();
  }
}

void Canbus::dump_config() { ESP_LOGCONFIG(TAG, "Canbus: can_id=%d", this->can_id_); }

void Canbus::send_data(uint32_t can_id, const std::vector<uint8_t> &data) {
  struct CanFrame can_message;

  uint8_t size = static_cast<uint8_t>(data.size());
  ESP_LOGD(TAG, "send canid=%d size=%d", can_id, size);
  if (size > CAN_MAX_DLC)
    size = CAN_MAX_DLC;
  can_message.can_dlc = size;
  can_message.can_id = can_id;

  for (int i = 0; i < size; i++) {
    can_message.data[i] = data[i];
    ESP_LOGVV(TAG, "  data[%d] = %02x", i, can_message.data[i]);
  }

  this->send_message(&can_message);
}

void Canbus::add_trigger(CanbusTrigger *trigger) {
  ESP_LOGVV(TAG, "add trigger for canid:%d", trigger->can_id_);
  this->triggers_.push_back(trigger);
};

void Canbus::loop() {
  struct CanFrame can_message;
  // readmessage
  if (this->read_message(&can_message) == canbus::ERROR_OK) {
    ESP_LOGD(TAG, "received can message can_id=%d size=%d", can_message.can_id, can_message.can_dlc);

    std::vector<uint8_t> data(&can_message.data[0], &can_message.data[can_message.can_dlc - 1]);

    // show data received
    for (int i = 0; i < can_message.can_dlc; i++)
      ESP_LOGV(TAG, "  data[%d]=%02x", i, can_message.data[i]);
    // fire all triggers
    for (auto trigger : this->triggers_) {
      if (trigger->can_id_ == can_message.can_id) {
        trigger->trigger(data);
      }
    }
  }
}

}  // namespace canbus
}  // namespace esphome
