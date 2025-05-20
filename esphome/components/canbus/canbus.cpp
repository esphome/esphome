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
    ESP_LOGCONFIG(TAG, "config extended id=0x%08" PRIx32, this->can_id_);
  } else {
    ESP_LOGCONFIG(TAG, "config standard id=0x%03" PRIx32, this->can_id_);
  }
}

void Canbus::send_data(uint32_t can_id, bool use_extended_id, bool remote_transmission_request,
                       const std::vector<uint8_t> &data) {
  struct CanFrame can_message;

  uint8_t size = static_cast<uint8_t>(data.size());
  if (use_extended_id) {
    ESP_LOGD(TAG, "send extended id=0x%08" PRIx32 " rtr=%s size=%d", can_id, TRUEFALSE(remote_transmission_request),
             size);
  } else {
    ESP_LOGD(TAG, "send standard id=0x%03" PRIx32 " rtr=%s size=%d", can_id, TRUEFALSE(remote_transmission_request),
             size);
  }
  if (size > CAN_MAX_DATA_LENGTH)
    size = CAN_MAX_DATA_LENGTH;
  can_message.can_data_length_code = size;
  can_message.can_id = can_id;
  can_message.use_extended_id = use_extended_id;
  can_message.remote_transmission_request = remote_transmission_request;

  for (int i = 0; i < size; i++) {
    can_message.data[i] = data[i];
    ESP_LOGVV(TAG, "  data[%d]=%02x", i, can_message.data[i]);
  }

  if (this->send_message(&can_message) != canbus::ERROR_OK) {
    if (use_extended_id) {
      ESP_LOGW(TAG, "send to extended id=0x%08" PRIx32 " failed!", can_id);
    } else {
      ESP_LOGW(TAG, "send to standard id=0x%03" PRIx32 " failed!", can_id);
    }
  }
}

void Canbus::add_trigger(CanbusTrigger *trigger) {
  if (trigger->use_extended_id_) {
    ESP_LOGVV(TAG, "add trigger for extended canid=0x%08" PRIx32, trigger->can_id_);
  } else {
    ESP_LOGVV(TAG, "add trigger for std canid=0x%03" PRIx32, trigger->can_id_);
  }
  this->triggers_.push_back(trigger);
};

void Canbus::loop() {
  struct CanFrame can_message;
  // read all messages until queue is empty
  int message_counter = 0;
  while (this->read_message(&can_message) == canbus::ERROR_OK) {
    message_counter++;
    if (can_message.use_extended_id) {
      ESP_LOGD(TAG, "received can message (#%d) extended can_id=0x%" PRIx32 " size=%d", message_counter,
               can_message.can_id, can_message.can_data_length_code);
    } else {
      ESP_LOGD(TAG, "received can message (#%d) std can_id=0x%" PRIx32 " size=%d", message_counter, can_message.can_id,
               can_message.can_data_length_code);
    }

    std::vector<uint8_t> data;

    // show data received
    for (int i = 0; i < can_message.can_data_length_code; i++) {
      ESP_LOGV(TAG, "  can_message.data[%d]=%02x", i, can_message.data[i]);
      data.push_back(can_message.data[i]);
    }

    this->callback_manager_(can_message.can_id, can_message.use_extended_id, can_message.remote_transmission_request,
                            data);

    // fire all triggers
    for (auto *trigger : this->triggers_) {
      if ((trigger->can_id_ == (can_message.can_id & trigger->can_id_mask_)) &&
          (trigger->use_extended_id_ == can_message.use_extended_id) &&
          (!trigger->remote_transmission_request_.has_value() ||
           trigger->remote_transmission_request_.value() == can_message.remote_transmission_request)) {
        trigger->trigger(data, can_message.can_id, can_message.remote_transmission_request);
      }
    }
  }
}

}  // namespace canbus
}  // namespace esphome
