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

canbus::Error Canbus::send_data(const CanFrame &frame) {
  if (frame.can_id > (frame.use_extended_id ? CAN_EXTENDED_ID_MAX : CAN_STANDARD_ID_MAX)) {
    ESP_LOGE(TAG, "CAN ID 0x%08" PRIx32 " exceeds frame identifier width", frame.can_id);
    return ERROR_FAILTX;
  }
#ifdef USE_CANBUS_FD
  if (frame.fd && !this->supports_fd_) {
    ESP_LOGE(TAG, "CAN FD frame on a bus without CAN FD");
    return ERROR_FAILTX;
  }
  if (frame.fd ? (frame.remote_transmission_request || can_dlc_to_len(frame.can_data_length_code) == CAN_INVALID_DLC)
               : frame.bit_rate_switch) {
    ESP_LOGE(TAG, "Invalid CAN frame: fd=%d rtr=%d brs=%d dlc=%" PRIu8, frame.fd, frame.remote_transmission_request,
             frame.bit_rate_switch, frame.can_data_length_code);
    return ERROR_FAILTX;
  }
#endif
  return this->send_frame_(frame);
}

#ifdef USE_CANBUS_FD
canbus::Error Canbus::send_data(uint32_t can_id, bool use_extended_id, bool remote_transmission_request,
                                const uint8_t *data, size_t len, bool fd, bool bit_rate_switch, bool pad_data) {
#else
canbus::Error Canbus::send_data(uint32_t can_id, bool use_extended_id, bool remote_transmission_request,
                                const uint8_t *data, size_t len) {
#endif
  CanFrame frame;
  frame.use_extended_id = use_extended_id;
  frame.remote_transmission_request = remote_transmission_request;
  frame.can_id = can_id;
#ifdef USE_CANBUS_FD
  frame.fd = fd;
  frame.bit_rate_switch = bit_rate_switch;
  frame.error_state_indicator = false;
  if (fd) {
    frame.can_data_length_code = can_len_to_dlc(len, pad_data);
    if (frame.can_data_length_code == CAN_INVALID_DLC) {
      ESP_LOGE(TAG, "CAN FD cannot carry %zu bytes without pad_data", len);
      return ERROR_FAILTX;
    }
    if (!remote_transmission_request && len != 0) {
      std::memcpy(frame.data, data, len);
      std::memset(frame.data + len, 0, can_dlc_to_len(frame.can_data_length_code) - len);
    }
    return this->send_data(frame);
  }
#endif
  if (len > CAN_MAX_DATA_LENGTH) {
    ESP_LOGW(TAG, "Classic CAN carries 8 bytes, %zu given, sent 8", len);
  }
  // Clamp before narrowing, so a lambda payload of 256 bytes or more cannot wrap to a short length
  frame.can_data_length_code = static_cast<uint8_t>(std::min<size_t>(len, CAN_MAX_DATA_LENGTH));
  // A remote request carries no data, so its bytes are zeroed rather than left from the stack. An empty vector may
  // hand over a null pointer, which memcpy must not get even for 0 bytes.
  if (remote_transmission_request) {
    std::memset(frame.data, 0, frame.can_data_length_code);
  } else if (frame.can_data_length_code != 0) {
    std::memcpy(frame.data, data, frame.can_data_length_code);
  }
#ifdef USE_CANBUS_FD
  // The caller's flags can still contradict each other
  return this->send_data(frame);
#else
  return this->send_frame_(frame);
#endif
}

bool Canbus::lambda_payload_fits_(const CanPayload &payload, [[maybe_unused]] bool fd) {
  if (!payload.truncated())
    return true;
#ifdef USE_CANBUS_FD
  if (fd) {
    ESP_LOGE(TAG, "CAN FD carries at most %u bytes, frame not sent", CAN_FRAME_DATA_LENGTH);
    return false;
  }
  // The payload still holds 64 bytes, so send_data() warns as it cuts a classic frame to 8
#else
  ESP_LOGW(TAG, "Classic CAN carries 8 bytes, the rest is cut");
#endif
  return true;
}

canbus::Error Canbus::send_frame_(const CanFrame &frame) {
  const uint8_t size = frame.payload_length();
  if (frame.use_extended_id) {
    ESP_LOGD(TAG, "send extended id=0x%08" PRIx32 " rtr=%s size=%d", frame.can_id,
             TRUEFALSE(frame.remote_transmission_request), size);
  } else {
    ESP_LOGD(TAG, "send standard id=0x%03" PRIx32 " rtr=%s size=%d", frame.can_id,
             TRUEFALSE(frame.remote_transmission_request), size);
  }
  ESP_LOGV_HEX(TAG, frame.data, size, CAN_FRAME_DATA_LENGTH, "  data=%s");

  // Drivers take a mutable pointer but do not change the frame
  canbus::Error error = this->send_message(const_cast<CanFrame *>(&frame));
  if (error != canbus::ERROR_OK) {
    if (frame.use_extended_id) {
      ESP_LOGW(TAG, "send to extended id=0x%08" PRIx32 " failed with error %d!", frame.can_id, error);
    } else {
      ESP_LOGW(TAG, "send to standard id=0x%03" PRIx32 " failed with error %d!", frame.can_id, error);
    }
  }
  return error;
}

void Canbus::loop() {
  uint32_t now = App.get_loop_component_start_time();
  if ((now - this->last_event_check_time_) >= EVENT_CHECK_INTERVAL_MS) {
    this->log_events_(this->get_events(), now);
    this->last_event_check_time_ = now;
  }

  struct CanFrame can_message;
  uint32_t message_counter = 0;
  for (; message_counter < this->max_frames_per_loop_; message_counter++) {
#ifdef USE_CANBUS_FD
    // Classic drivers leave the CAN FD flags alone
    can_message.fd = false;
    can_message.bit_rate_switch = false;
    can_message.error_state_indicator = false;
#endif
    if (this->read_message(&can_message) != canbus::ERROR_OK) {
      break;
    }

    const uint8_t size = can_message.payload_length();
    if (can_message.use_extended_id) {
      ESP_LOGD(TAG, "received can message (#%" PRIu32 ") extended can_id=0x%" PRIx32 " size=%d", message_counter + 1,
               can_message.can_id, size);
    } else {
      ESP_LOGD(TAG, "received can message (#%" PRIu32 ") std can_id=0x%" PRIx32 " size=%d", message_counter + 1,
               can_message.can_id, size);
    }

    ESP_LOGV_HEX(TAG, can_message.data, size, CAN_FRAME_DATA_LENGTH, "  data=%s");
    this->frame_callbacks_.call(can_message);
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
