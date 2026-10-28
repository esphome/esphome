#include "canbus.h"
#include <algorithm>
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::canbus {

ESPHOME_LOG_TAG(TAG, "canbus");

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

canbus::Error Canbus::send_data(CanFrame &frame) {
  const auto size = can_dlc_to_len(frame.can_data_length_code);
  if (size == CAN_INVALID_DLC || (!frame.fd && size > CAN_MAX_DATA_LENGTH)) {
    ESP_LOGE(TAG, "Invalid DLC=%" PRIu8 " for CAN frame format", frame.can_data_length_code);
    return ERROR_FAILTX;
  }
  if (frame.can_id > (frame.use_extended_id ? 0x1FFFFFFFU : 0x7FFU)) {
    ESP_LOGE(TAG, "CAN ID 0x%08" PRIx32 " exceeds frame identifier width", frame.can_id);
    return ERROR_FAILTX;
  }
  if (frame.fd && frame.remote_transmission_request) {
    ESP_LOGE(TAG, "Invalid CAN frame: FD with RTR");
    return ERROR_FAILTX;
  }
  if (!frame.fd && frame.bit_rate_switch) {
    ESP_LOGE(TAG, "Invalid CAN frame: BRS without FD");
    return ERROR_FAILTX;
  }
  if (frame.fd && !this->supports_fd()) {
    ESP_LOGE(TAG, "CAN FD is not supported by this controller or mode");
    return ERROR_FAILTX;
  }
  if (frame.use_extended_id) {
    ESP_LOGD(TAG, "send extended id=0x%08" PRIx32 " rtr=%s size=%d", frame.can_id,
             TRUEFALSE(frame.remote_transmission_request), size);
  } else {
    ESP_LOGD(TAG, "send standard id=0x%03" PRIx32 " rtr=%s size=%d", frame.can_id,
             TRUEFALSE(frame.remote_transmission_request), size);
  }

  for (int i = 0; !frame.remote_transmission_request && i < size; i++) {
    ESP_LOGVV(TAG, "  data[%d]=%02x", i, frame.data[i]);
  }

  canbus::Error error = this->send_message(&frame);
  if (error != canbus::ERROR_OK) {
    if (frame.use_extended_id) {
      ESP_LOGW(TAG, "send to extended id=0x%08" PRIx32 " failed with error %d!", frame.can_id, error);
    } else {
      ESP_LOGW(TAG, "send to standard id=0x%03" PRIx32 " failed with error %d!", frame.can_id, error);
    }
  }
  return error;
}

canbus::Error Canbus::send_data(uint32_t can_id, bool use_extended_id, bool remote_transmission_request,
                                const uint8_t *data, size_t len, bool fd, bool bit_rate_switch, bool pad_data) {
  if (len > (fd ? CAN_FD_MAX_DATA_LENGTH : CAN_MAX_DATA_LENGTH)) {
    ESP_LOGE(TAG, "CAN payload length %zu exceeds maximum of %zu bytes", len,
             static_cast<size_t>(fd ? CAN_FD_MAX_DATA_LENGTH : CAN_MAX_DATA_LENGTH));
    return ERROR_FAILTX;
  }
  const uint8_t dlc = can_len_to_dlc(len, pad_data);
  if (dlc == CAN_INVALID_DLC) {
    ESP_LOGE(TAG, "CAN FD payload length %zu requires pad_data: true", len);
    return ERROR_FAILTX;
  }
  if (len != 0 && data == nullptr && !remote_transmission_request) {
    ESP_LOGE(TAG, "CAN payload data is null");
    return ERROR_FAILTX;
  }

  CanFrame can_message{};
  can_message.can_data_length_code = dlc;
  can_message.can_id = can_id;
  can_message.use_extended_id = use_extended_id;
  can_message.remote_transmission_request = remote_transmission_request;
  can_message.fd = fd;
  can_message.bit_rate_switch = bit_rate_switch;

  for (size_t i = 0; !remote_transmission_request && i < len; i++) {
    can_message.data[i] = data[i];
  }

  return this->send_data(can_message);
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
  uint32_t now = App.get_loop_component_start_time();
  if ((now - this->last_event_check_time_) >= EVENT_CHECK_INTERVAL_MS) {
    this->log_events_(this->get_events(), now);
    this->last_event_check_time_ = now;
  }

  uint32_t message_counter = 0;
  for (; message_counter < this->max_frames_per_loop_; message_counter++) {
    struct CanFrame can_message{};
    if (this->read_message(&can_message) != canbus::ERROR_OK) {
      break;
    }

    uint8_t size = can_dlc_to_len(can_message.can_data_length_code);
    if (size == CAN_INVALID_DLC) {
      ESP_LOGW(TAG, "Ignoring CAN frame with invalid DLC=%" PRIu8, can_message.can_data_length_code);
      continue;
    }
    if (!can_message.fd) {
      size = std::min(size, CAN_MAX_DATA_LENGTH);
    }

    if (can_message.use_extended_id) {
      ESP_LOGD(TAG, "received can message (#%" PRIu32 ") ext can_id=0x%" PRIx32 " size=%d", message_counter + 1,
               can_message.can_id, size);
    } else {
      ESP_LOGD(TAG, "received can message (#%" PRIu32 ") std can_id=0x%" PRIx32 " size=%d", message_counter + 1,
               can_message.can_id, size);
    }

    std::vector<uint8_t> data(can_message.data, can_message.data + size);

    // show data received when LOG_LEVEL >= VERBOSE
    ESP_LOGV_HEX(TAG, can_message.data, size, CAN_FD_MAX_DATA_LENGTH, "  Data: %s");

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

  if (message_counter == this->max_frames_per_loop_) {
    ESP_LOGW(TAG,
             "Reached max_frames_per_loop=%" PRIu32 ", deferring remaining CAN frames. Some frames might be dropped",
             this->max_frames_per_loop_);
  }
}

void Canbus::log_events_(CanEventFlags events, uint32_t now) {
  // Bus-off gets its own handling because automatic recovery can make it flap
  if (events & CanEventFlags::CAN_EVENT_BUS_OFF) {
    this->last_bus_off_time_ = now;
    if (!this->bus_off_) {
      this->bus_off_ = true;
      this->status_set_warning(LOG_STR("Bus off"));
    }
  } else if (this->bus_off_) {
    if ((now - this->last_bus_off_time_) >= EVENT_LOG_BUS_OFF_HOLDOFF_MS) {
      // No bus-off event for the hold-off time, so ask whether it is really over
      if (this->get_status().bus_off) {
        this->last_bus_off_time_ = now;
      } else {
        this->bus_off_ = false;
        this->status_clear_warning();
        // Events gathered while bus-off say nothing about the recovered bus
        this->events_to_log_ = 0;
      }
    }
  } else {
    this->events_to_log_ |= events;
    if (this->events_to_log_ != 0 && (now - this->last_event_log_time_) >= EVENT_LOG_THROTTLE_MS) {
      if (this->events_to_log_ & (CanEventFlags::CAN_EVENT_PASSIVE | CanEventFlags::CAN_EVENT_ACTIVE)) {
        bool error_passive = this->events_to_log_ & CanEventFlags::CAN_EVENT_PASSIVE;
        if (error_passive && (this->events_to_log_ & CanEventFlags::CAN_EVENT_ACTIVE)) {
          // Both edges within one throttle window, so the counters decide the current state
          auto status = this->get_status();
          error_passive = status.rx_error_counter >= CAN_ERROR_PASSIVE_THRESHOLD ||
                          status.tx_error_counter >= CAN_ERROR_PASSIVE_THRESHOLD;
        }
        ESP_LOGW(TAG, "entered %s", error_passive ? LOG_STR_LITERAL("error-passive") : LOG_STR_LITERAL("error-active"));
      }
      if (this->events_to_log_ & CanEventFlags::CAN_EVENT_RX_QUEUE_FULL) {
        ESP_LOGD(TAG, "receive buffer overrun");
      }
      this->events_to_log_ = 0;
      this->last_event_log_time_ = now;
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
