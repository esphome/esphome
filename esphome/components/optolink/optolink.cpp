#ifdef USE_ARDUINO

#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "optolink.h"

VitoWiFiClass<USE_OPTOLINK_VITOWIFI_PROTOCOL> VitoWiFi;  // NOLINT

namespace esphome {
namespace optolink {

static const char *const TAG = "optolink";

void Optolink::setup() {
  ESP_LOGI(TAG, "setup");

  if (logger_enabled_) {
    VitoWiFi.setLogger(this);
    VitoWiFi.enableLogger();
  }

#if defined(USE_ESP32)
  VitoWiFi.setup(&Serial, rx_pin_, tx_pin_);
#elif defined(USE_ESP8266)
  VitoWiFi.setup(&Serial);
#endif
}

void Optolink::loop() {
  communication_check_();
  if (!communication_suspended()) {
    VitoWiFi.loop();
  }
}

int Optolink::get_queue_size() { return VitoWiFi.queueSize(); }

void Optolink::set_state_(const char *format, ...) {
  va_list args;
  va_start(args, format);
  char buffer[128];
  std::vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  state_ = buffer;
}

void Optolink::notify_receive() { timestamp_receive_ = timestamp_loop_; }

void Optolink::notify_send() { timestamp_send_ = timestamp_loop_; }

void Optolink::communication_check_() {
  timestamp_loop_ = millis();

  if (communication_suspended()) {
    if (timestamp_loop_ < timestamp_disruption_ ||
        (timestamp_loop_ - timestamp_disruption_) > COMMUNICATION_SUSPENSION_DURATION) {
      resume_communication_();
    } else {
      set_state_("communication suspended");
      // if (timestamp_loop % 10 == 0)
      //   ESP_LOGD(TAG, "communication suspended");
    }
  } else if (timestamp_loop_ < timestamp_send_ || timestamp_loop_ < timestamp_receive_) {
    ESP_LOGI(TAG, "timestamp rollover");
    timestamp_send_ = 0;
    timestamp_receive_ = 0;
  } else if (timestamp_send_ > 0 && (timestamp_loop_ - timestamp_receive_) > COMMUNICATION_CHECK_WINDOW) {
    // last response older than 10 sec - check if there was no request in same time window except last two seconds
    if (timestamp_send_ > timestamp_loop_ - MAX_RESPONSE_DELAY) {
      // request too fresh -> possiblly still waiting for response
    } else if (timestamp_send_ < timestamp_receive_) {
      // no new and fresh request since last response
      set_state_("communication unused");
    } else {
      suspend_communication_();
    }
  } else {
    set_state_("communication active");
  }
}

void Optolink::suspend_communication_() {
  set_state_("communication suspended");
  ESP_LOGW(TAG,
           "communication disrupted - suspending communication for 10 sec; timestamp_loop: %u, timestamp_send: %u,  "
           "timestamp_receive: %u ",
           timestamp_loop_, timestamp_send_, timestamp_receive_);
  timestamp_disruption_ = timestamp_loop_;
}

void Optolink::resume_communication_() {
  ESP_LOGI(TAG, "resuming communication");
  timestamp_disruption_ = 0;
  timestamp_send_ = 0;
  timestamp_receive_ = 0;
}

bool Optolink::communication_suspended() { return (timestamp_disruption_ != 0); }

bool Optolink::read_value(IDatapoint *datapoint) {
  if (datapoint != nullptr && !communication_suspended()) {
    ESP_LOGI(TAG, "requesting value of datapoint %s", datapoint->getName());
    if (VitoWiFi.readDatapoint(*datapoint)) {
      notify_send();
    } else {
      ESP_LOGE(TAG, "read request rejected due to queue overload - queue size: %d", VitoWiFi.queueSize());
      suspend_communication_();
      for (auto *dp : IDatapoint::getCollection()) {
        ESP_LOGD(TAG, "queued datapoint: %s", dp->getName());
      }
      return false;
    }
  }
  return true;
}

bool Optolink::write_value(IDatapoint *datapoint, DPValue dp_value) {
  if (datapoint != nullptr && !communication_suspended()) {
    char buffer[64];
    dp_value.getString(buffer, sizeof(buffer));
    ESP_LOGI(TAG, "sending value %s of datapoint %s", buffer, datapoint->getName());
    if (VitoWiFi.writeDatapoint(*datapoint, dp_value)) {
      notify_send();
    } else {
      ESP_LOGE(TAG, "write request rejected due to queue overload - queue size: %d", VitoWiFi.queueSize());
      for (auto *dp : IDatapoint::getCollection()) {
        ESP_LOGE(TAG, "queued dp: %s", dp->getName());
      }
      return false;
    }
  }
  return true;
}

size_t Optolink::write(uint8_t ch) {
  if (ch == '\n') {
    ESP_LOGD(TAG, "VitoWiFi: %s", log_buffer_.c_str());
    log_buffer_.clear();
  } else {
    log_buffer_.push_back(ch);
  }
  return 1;
}

}  // namespace optolink
}  // namespace esphome

#endif
