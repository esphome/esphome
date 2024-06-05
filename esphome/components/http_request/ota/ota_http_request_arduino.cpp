#include "ota_http_request.h"
#include "watchdog.h"

#ifdef USE_ARDUINO
#include "ota_http_request_arduino.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/network/util.h"
#include "esphome/components/md5/md5.h"

namespace esphome {
namespace http_request {

struct Header {
  const char *name;
  const char *value;
};

void OtaHttpRequestComponentArduino::http_init(const std::string &url) {
  const char *header_keys[] = {"Content-Length", "Content-Type"};
  const size_t header_count = sizeof(header_keys) / sizeof(header_keys[0]);
  watchdog::WatchdogManager wdts;

#ifdef USE_ESP8266
  if (this->stream_ptr_ == nullptr && this->set_stream_ptr_()) {
    ESP_LOGE(TAG, "Unable to set client");
    return;
  }
#endif  // USE_ESP8266

#ifdef USE_RP2040
  this->client_.setInsecure();
#endif

  App.feed_wdt();

#if defined(USE_ESP32) || defined(USE_RP2040)
  this->status_ = this->client_.begin(url.c_str());
#endif
#ifdef USE_ESP8266
  this->client_.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  this->status_ = this->client_.begin(*this->stream_ptr_, url.c_str());
#endif

  if (!this->status_) {
    this->client_.end();
    return;
  }

  this->client_.setReuse(true);

  // returned needed headers must be collected before the requests
  this->client_.collectHeaders(header_keys, header_count);

  // HTTP GET
  this->status_ = this->client_.GET();

  this->body_length_ = (size_t) this->client_.getSize();

#if defined(USE_ESP32) || defined(USE_RP2040)
  if (this->stream_ptr_ == nullptr) {
    this->set_stream_ptr_();
  }
#endif
}

int OtaHttpRequestComponentArduino::http_read(uint8_t *buf, const size_t max_len) {
#ifdef USE_ESP8266
#if USE_ARDUINO_VERSION_CODE >= VERSION_CODE(3, 1, 0)  // && USE_ARDUINO_VERSION_CODE < VERSION_CODE(?, ?, ?)
  if (!this->secure_()) {
    ESP_LOGW(TAG, "Using HTTP on Arduino version >= 3.1 is **very** slow. Consider setting framework version to 3.0.2 "
                  "in your YAML, or use HTTPS");
  }
#endif  // USE_ARDUINO_VERSION_CODE
#endif  // USE_ESP8266

  watchdog::WatchdogManager wdts;

  // Since arduino8266 >= 3.1 using this->stream_ptr_ is broken (https://github.com/esp8266/Arduino/issues/9035)
  WiFiClient *stream_ptr = this->client_.getStreamPtr();
  if (stream_ptr == nullptr) {
    ESP_LOGE(TAG, "Stream pointer vanished!");
    return -1;
  }

  int available_data = stream_ptr->available();
  int bufsize = std::min((int) max_len, available_data);
  if (bufsize > 0) {
    stream_ptr->readBytes(buf, bufsize);
    this->bytes_read_ += bufsize;
    buf[bufsize] = '\0';  // not fed to ota
  }

  return bufsize;
}

void OtaHttpRequestComponentArduino::http_end() {
  watchdog::WatchdogManager wdts;
  this->client_.end();
}

int OtaHttpRequestComponentArduino::set_stream_ptr_() {
#ifdef USE_ESP8266
#ifdef USE_HTTP_REQUEST_ESP8266_HTTPS
  if (this->secure_()) {
    ESP_LOGV(TAG, "ESP8266 HTTPS connection with WiFiClientSecure");
    this->stream_ptr_ = std::make_unique<WiFiClientSecure>();
    WiFiClientSecure *secure_client = static_cast<WiFiClientSecure *>(this->stream_ptr_.get());
    secure_client->setBufferSizes(this->max_http_recv_buffer_, 512);
    secure_client->setInsecure();
  } else {
    this->stream_ptr_ = std::make_unique<WiFiClient>();
  }
#else
  ESP_LOGV(TAG, "ESP8266 HTTP connection with WiFiClient");
  if (this->secure_()) {
    ESP_LOGE(TAG, "Can't use HTTPS connection with esp8266_disable_ssl_support");
    return -1;
  }
  this->stream_ptr_ = std::make_unique<WiFiClient>();
#endif  // USE_HTTP_REQUEST_ESP8266_HTTPS
#endif  // USE_ESP8266

#if defined(USE_ESP32) || defined(USE_RP2040)
  this->stream_ptr_ = std::unique_ptr<WiFiClient>(this->client_.getStreamPtr());
#endif
  return 0;
}

}  // namespace http_request
}  // namespace esphome

#endif  // USE_ARDUINO
