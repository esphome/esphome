#include "canbus.h"
#include "esphome/core/log.h"

namespace esphome {
namespace canbus {

static const char *const TAG = "canbus";

void Canbus::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Canbus...");
  if (!this->setup_internal()) {
    ESP_LOGE(TAG, "setup error!");
    this->mark_failed();
  }
}

void Canbus::dump_config() {
  if (this->use_extended_id_) {
    ESP_LOGCONFIG(TAG, "config extended id=0x%08x", this->can_id_);
  } else {
    ESP_LOGCONFIG(TAG, "config standard id=0x%03x", this->can_id_);
  }
}

void Canbus::send_data(uint32_t can_id, bool use_extended_id, const std::vector<uint8_t> &data) {
  struct CanFrame can_message;

  uint8_t size = static_cast<uint8_t>(data.size());
  if (use_extended_id) {
    ESP_LOGD(TAG, "send extended id=0x%08x size=%d", can_id, size);
  } else {
    ESP_LOGD(TAG, "send extended id=0x%03x size=%d", can_id, size);
  }
  if (size > CAN_MAX_DATA_LENGTH)
    size = CAN_MAX_DATA_LENGTH;
  can_message.can_data_length_code = size;
  can_message.can_id = can_id;
  can_message.use_extended_id = use_extended_id;

  for (int i = 0; i < size; i++) {
    can_message.data[i] = data[i];
    ESP_LOGVV(TAG, "  data[%d]=%02x", i, can_message.data[i]);
  }

  this->send_message(&can_message);
}

void Canbus::add_trigger(CanbusTrigger *trigger) {
  if (trigger->use_extended_id_) {
    ESP_LOGVV(TAG, "add trigger for extended canid=0x%08x", trigger->can_id_);
  } else {
    ESP_LOGVV(TAG, "add trigger for std canid=0x%03x", trigger->can_id_);
  }
  this->triggers_.push_back(trigger);
};

void Canbus::loop() {
  struct CanFrame can_message;
  // readmessage
  if (this->read_message(&can_message) == canbus::ERROR_OK) {
    if (can_message.use_extended_id) {
      ESP_LOGD(TAG, "received can message extended can_id=0x%x size=%d", can_message.can_id,
               can_message.can_data_length_code);
    } else {
      ESP_LOGD(TAG, "received can message std can_id=0x%x size=%d", can_message.can_id,
               can_message.can_data_length_code);
    }

    std::vector<uint8_t> data;

    // show data received
    for (int i = 0; i < can_message.can_data_length_code; i++) {
      ESP_LOGV(TAG, "  can_message.data[%d]=%02x", i, can_message.data[i]);
      data.push_back(can_message.data[i]);
    }

    // fire all triggers
    for (auto trigger : this->triggers_) {
      if ((trigger->can_id_ == can_message.can_id) && (trigger->use_extended_id_ == can_message.use_extended_id)) {
        trigger->trigger(data);
      }
    }
  }
}

}  // namespace canbus
}  // namespace esphome
