#ifdef USE_ARDUINO

#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "optolink.h"

#if defined(VITOWIFI_PROTOCOL)
// NOLINTNEXTLINE
VitoWiFiClass<VITOWIFI_PROTOCOL> VitoWiFi;  // VITOWIFI_PROTOCOL always is set
#else
// NOLINTNEXTLINE
VitoWiFiClass<P300> VitoWiFi;  // this is not really a fallback but dedicated to clang-lint
#endif

namespace esphome {
namespace optolink {

static const char *const TAG = "optolink";

void Optolink::comm_() {
  ESP_LOGD(TAG, "enter _comm");
  VitoWiFi.readAll();
  ESP_LOGD(TAG, "exit _comm");
}

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
  // ESP_LOGD(TAG, "queue size: %d", VitoWiFi.queueSize());
  VitoWiFi.loop();
}

void Optolink::set_state(const char *format, ...) {
  va_list args;
  va_start(args, format);
  char buffer[128];
  std::vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  state_ = buffer;
}

bool Optolink::read_value(IDatapoint *datapoint) {
  if (datapoint != nullptr) {
    ESP_LOGI(TAG, "requesting value of datapoint %s", datapoint->getName());
    if (!VitoWiFi.readDatapoint(*datapoint)) {
      ESP_LOGE(TAG, "read request rejected due to queue overload - queue size: %d", VitoWiFi.queueSize());
      for (auto *dp : IDatapoint::getCollection()) {
        ESP_LOGD(TAG, "queued datapoint: %s", dp->getName());
      }
      return false;
    }
  }
  return true;
}

bool Optolink::write_value(IDatapoint *datapoint, DPValue dp_value) {
  if (datapoint != nullptr) {
    char buffer[64];
    dp_value.getString(buffer, sizeof(buffer));
    ESP_LOGI(TAG, "sending value %s of datapoint %s", buffer, datapoint->getName());
    if (!VitoWiFi.writeDatapoint(*datapoint, dp_value)) {
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
