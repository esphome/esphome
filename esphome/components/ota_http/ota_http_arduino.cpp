// firmware update from http (ie when OTA port is behind a firewall)
// code adapted from
// esphome/components/ota/ota_backend.cpp
// and
// esphome/components/http_request

#include "ota_http.h"

#ifdef USE_ARDUINO
#include "ota_http_arduino.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/network/util.h"
#include "esphome/components/md5/md5.h"

namespace esphome {
namespace ota_http {

struct Header {
  const char *name;
  const char *value;
};

int OtaHttpArduino::http_init() {
  int http_code;
  uint32_t start_time;
  uint32_t duration;

  const char *header_keys[] = {"Content-Length", "Content-Type"};
  const size_t header_count = sizeof(header_keys) / sizeof(header_keys[0]);

#ifdef USE_ESP8266
#ifdef USE_HTTP_REQUEST_ESP8266_HTTPS
  if (this->secure_()) {
    ESP_LOGD(TAG, "esp8266 https connection with WiFiClientSecure");
    this->stream_ptr_ = std::make_unique<WiFiClientSecure>();
    WiFiClientSecure *secure_client = static_cast<WiFiClientSecure *>(this->stream_ptr_.get());
    secure_client->setBufferSizes(this->max_http_recv_buffer_, 512);
    secure_client->setInsecure();
  } else {
    this->stream_ptr_ = std::make_unique<WiFiClient>();
  }
#else
  ESP_LOGD(TAG, "esp8266 http connection with WiFiClient");
  if (this->secure_()) {
    ESP_LOGE(TAG, "Can't use https connection with esp8266_disable_ssl_support");
    return -1;
  }
  this->stream_ptr_ = std::make_unique<WiFiClient>();
#endif  // USE_HTTP_REQUEST_ESP8266_HTTPS
#endif  // USE_ESP8266

  ESP_LOGD(TAG, "Trying to connect to %s", pref_.url);

  bool status = false;
#if defined(USE_ESP32) || defined(USE_RP2040)
  status = this->client_.begin(pref_.url);
#endif
#ifdef USE_ESP8266
  status = this->client_.begin(*this->stream_ptr_, pref_.url);
#endif

  if (!status) {
    ESP_LOGE(TAG, "Unable to make http connection");
    this->client_.end();
    return -1;
  } else {
    ESP_LOGV(TAG, "http begin successfull.");
  }

  this->client_.setReuse(true);
  ESP_LOGVV(TAG, "http client setReuse.");

  // returned needed headers must be collected before the requests
  this->client_.collectHeaders(header_keys, header_count);
  ESP_LOGV(TAG, "http headers collected.");

  // http GET
  start_time = millis();
  http_code = this->client_.GET();
  duration = millis() - start_time;
  ESP_LOGV(TAG, "http GET finished.");

  if (http_code >= 310) {
    ESP_LOGW(TAG, "HTTP Request failed; URL: %s; Error: %s (%d); Duration: %u ms", pref_.url,
             HTTPClient::errorToString(http_code).c_str(), http_code, duration);
    return -1;
  }

  if (this->client_.getSize() < 0) {
    ESP_LOGE(TAG, "Incorrect file size (%d) reported by http server (http status: %d). Aborting",
             this->client_.getSize(), http_code);
    return -1;
  }

  this->body_length_ = (size_t) this->client_.getSize();
  ESP_LOGD(TAG, "firmware is %d bytes length.", this->body_length_);

#if defined(USE_ESP32) || defined(USE_RP2040)
  this->stream_ptr_ = std::unique_ptr<WiFiClient>(this->client_.getStreamPtr());
#endif

  return 1;
}

size_t OtaHttpArduino::http_read(uint8_t *buf, const size_t max_len) {
  // wait for the stream to be populated
  while (this->stream_ptr_->available() == 0) {
    // give other tasks a chance to run while waiting for some data:
    // ESP_LOGVV(TAG, "not enougth data available: %zu (total read: %zu)", streamPtr->available(), bytes_read);
    yield();
    delay(1);
  }
  // size_t bufsize = std::min(max_len, this->body_length - this->bytes_read);
  int available_data = this->stream_ptr_->available();
  if (available_data < 0) {
    ESP_LOGE(TAG, "ERROR: stream closed");
    this->cleanup_();
    return -1;
  }
  size_t bufsize = std::min(max_len, (size_t) available_data);

  // ESP_LOGVV(TAG, "data available: %zu", available_data);

  this->stream_ptr_->readBytes(buf, bufsize);
  this->bytes_read_ += bufsize;
  buf[bufsize] = '\0';  // not fed to ota

  return bufsize;
}

void OtaHttpArduino::http_end() { this->client_.end(); }

}  // namespace ota_http
}  // namespace esphome

#endif  // USE_ARDUINO
