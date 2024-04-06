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

void OtaHttpArduino::http_init() {
  const char *header_keys[] = {"Content-Length", "Content-Type"};
  const size_t header_count = sizeof(header_keys) / sizeof(header_keys[0]);

#ifdef USE_ESP8266
  if (this->stream_ptr_ == nullptr && this->set_stream_ptr_()) {
    ESP_LOGE(TAG, "Unable to set client");
  }
#endif  // USE_ESP8266

#ifdef USE_RP2040
  this->client_.setInsecure();
#endif

  App.feed_wdt();

#if defined(USE_ESP32) || defined(USE_RP2040)
  this->status_ = this->client_.begin(this->url_);
#endif
#ifdef USE_ESP8266
  this->status_ = this->client_.begin(*this->stream_ptr_, String(url));
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

int OtaHttpArduino::http_read(uint8_t *buf, const size_t max_len) {
  // wait for the stream to be populated
  while (this->stream_ptr_->available() == 0) {
    // give other tasks a chance to run while waiting for some data:
    App.feed_wdt();
    yield();
    delay(1);
  }
  int available_data = this->stream_ptr_->available();
  int bufsize = std::min((int) max_len, available_data);
  if (bufsize > 0) {
    this->stream_ptr_->readBytes(buf, bufsize);
    this->bytes_read_ += bufsize;
    buf[bufsize] = '\0';  // not fed to ota
  }

  return bufsize;
}

void OtaHttpArduino::http_end() { this->client_.end(); }

int OtaHttpArduino::set_stream_ptr_() {
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

}  // namespace ota_http
}  // namespace esphome

#endif  // USE_ARDUINO
