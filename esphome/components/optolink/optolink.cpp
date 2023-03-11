#include "esphome/core/defines.h"
#include "esphome/components/optolink/optolink.h"
#include <VitoWiFi.h>

VitoWiFiClass<VITOWIFI_PROTOCOL> VitoWiFi;

namespace esphome {
namespace optolink {

void Optolink::_comm() {
  ESP_LOGD("Optolink", "enter _comm");
  VitoWiFi.readAll();
  ESP_LOGD("Optolink", "exit _comm");
}

void Optolink::setup() {
  ESP_LOGI("Optolink", "setup");

  if (logger_enabled_) {
    VitoWiFi.setLogger(this);
    VitoWiFi.enableLogger();
  }

#if defined(USE_ESP32)
  VitoWiFi.setup(&Serial, rx_pin_, tx_pin_);
#elif defined(USE_ESP8266)
  VitoWiFi.setup(&Serial);
#endif

  // set_interval("Optolink_comm", 10000, std::bind(&Optolink::_comm, this));
}

void Optolink::loop() { VitoWiFi.loop(); }

void Optolink::set_error(const std::string &format, ...) {
  va_list args;
  va_start(args, format);
  char buffer[128];
  size_t n = std::vsnprintf(buffer, sizeof(buffer), format.c_str(), args);
  va_end(args);

  error_ = buffer;
}

void Optolink::read_value(IDatapoint *datapoint) {
  if (datapoint != nullptr) {
    ESP_LOGI("Optolink", " read value of datapoint %s", datapoint->getName());
    VitoWiFi.readDatapoint(*datapoint);
  }
}

void Optolink::write_value(IDatapoint *datapoint, DPValue dpValue) {
  if (datapoint != nullptr) {
    char buffer[64];
    dpValue.getString(buffer, sizeof(buffer));
    ESP_LOGI("Optolink", " write value %s of datapoint %s", buffer, datapoint->getName());
    VitoWiFi.writeDatapoint(*datapoint, dpValue);
  }
}

size_t Optolink::write(uint8_t ch) {
  if (ch == '\n') {
    ESP_LOGD("VitoWifi", "%s", log_buffer_.c_str());
    log_buffer_.clear();
  } else {
    log_buffer_.push_back(ch);
  }
  return 1;
}

}  // namespace optolink
}  // namespace esphome
