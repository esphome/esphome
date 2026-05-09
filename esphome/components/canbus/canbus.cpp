#include "canbus.h"
#include <algorithm>
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::canbus {

static const char *const TAG = "canbus";

void Canbus::setup() {
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

canbus::Error Canbus::send_data(uint32_t can_id, bool use_extended_id, bool remote_transmission_request,
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

  canbus::Error error = this->send_message(&can_message);
  if (error != canbus::ERROR_OK) {
    if (use_extended_id) {
      ESP_LOGW(TAG, "send to extended id=0x%08" PRIx32 " failed with error %d!", can_id, error);
    } else {
      ESP_LOGW(TAG, "send to standard id=0x%03" PRIx32 " failed with error %d!", can_id, error);
    }
  }
  return error;
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
  enum CanEventFlags events = this->get_events();

  this->log_events_(events);

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
    for (int i = 0; i < std::min(can_message.can_data_length_code, CAN_MAX_DATA_LENGTH); i++) {
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

void Canbus::log_events_(CanEventFlags events) {
  uint32_t now = millis();

  // special handling for bus-off because that can switch on or off constantly due to automatic bus-off-recovery.
  if (events & CanEventFlags::CAN_EVENT_BUS_OFF) {
    this->last_bus_off_time_ = now;
    if (!this->bus_off_) {
      ESP_LOGW(TAG, "entered bus off");
      this->bus_off_ = true;
    }
  } else if (this->bus_off_) {
    if ((now - this->last_bus_off_time_) >= EVENT_LOG_BUS_OFF_HOLDOFF_MS) {
      // hasn't thrown bus-off event for holdoff time -> check if we're still bus-off
      auto status = this->get_status();
      if (status.bus_off) {
        // still bus-off
        this->last_bus_off_time_ = now;
      } else {
        ESP_LOGW(TAG, "recovered from bus off");
        this->bus_off_ = false;

        // clear events_to_log as they are meaningless after bus-recovery
        this->events_to_log_ = 0;
      }
    }
  } else {
    this->events_to_log_ |= events;

    if ((now - this->last_event_log_time_) >= EVENT_LOG_THROTTLE_MS) {
      bool logged_event = false;

      if (this->events_to_log_ & CanEventFlags::CAN_EVENT_PASSIVE) {
        ESP_LOGW(TAG, "entered error-passive");
        logged_event = true;
      }
      if (this->events_to_log_ & CanEventFlags::CAN_EVENT_ACTIVE) {
        ESP_LOGW(TAG, "entered error-active");
        logged_event = true;
      }
      if (this->events_to_log_ & CanEventFlags::CAN_EVENT_RX_QUEUE_FULL) {
        ESP_LOGD(TAG, "receive buffer overrun");
        logged_event = true;
      }

      this->events_to_log_ = 0;
      if (logged_event) {
        this->last_event_log_time_ = now;
      }
    }
  }

#ifdef ESPHOME_LOG_HAS_VERBOSE
  if ((now - this->last_state_log_time_) >= STATE_LOG_INTERVAL_MS) {
    auto status = this->get_status();
    ESP_LOGV(TAG, "Status: bus_off %d, tx_err %d, rx_err %d", status.bus_off, status.tx_error_counter,
             status.rx_error_counter);
    this->last_state_log_time_ = now;
  }
#endif
}

}  // namespace esphome::canbus
