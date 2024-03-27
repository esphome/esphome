#include "ota_http.h"

#ifdef USE_ARDUINO
#include "ota_http_arduino.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/network/util.h"
#include "esphome/components/md5/md5.h"
#ifdef USE_ESP32
#include "esp_task_wdt.h"
#include "esp_idf_version.h"
#endif

namespace esphome {
namespace ota_http {

struct Header {
  const char *name;
  const char *value;
};

int OtaHttpArduino::http_init(char *url) {
  int http_code;
  uint32_t start_time;
  uint32_t duration;

  const char *header_keys[] = {"Content-Length", "Content-Type"};
  const size_t header_count = sizeof(header_keys) / sizeof(header_keys[0]);

#ifdef USE_ESP8266
  EspClass::wdtEnable(WDT_TIMEOUT_S * 1000);
  if (this->stream_ptr_ == nullptr && this->set_stream_ptr_()) {
    ESP_LOGE(TAG, "Unable to set client");
  }
#endif  // USE_ESP8266

#ifdef USE_ESP32
#if ESP_IDF_VERSION_MAJOR >= 5
  esp_task_wdt_config_t wdt_config = {
      .timeout_ms = WDT_TIMEOUT_S * 1000,
      .idle_core_mask = 0x03,
      .trigger_panic = true,
  };
  esp_task_wdt_reconfigure(&wdt_config);
#else
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
#endif  // ESP_IDF_VERSION_MAJOR
#endif  // USE_ESP32

  ESP_LOGD(TAG, "Connecting to %s", url);

  bool status = false;
#ifdef USE_RP2040
  watchdog_enable(WDT_TIMEOUT_S * 1000, true);
  this->client_.setInsecure();
#endif

  App.feed_wdt();

#if defined(USE_ESP32) || defined(USE_RP2040)
  status = this->client_.begin(url);
#endif
#ifdef USE_ESP8266
  status = this->client_.begin(*this->stream_ptr_, String(url));
#endif

  if (!status) {
    ESP_LOGE(TAG, "Unable to make HTTP connection");
    this->client_.end();
    return -1;
  } else {
    ESP_LOGV(TAG, "HTTP begin successful");
  }

  this->client_.setReuse(true);
  ESP_LOGVV(TAG, "HTTP client setReuse");

  // returned needed headers must be collected before the requests
  this->client_.collectHeaders(header_keys, header_count);
  ESP_LOGV(TAG, "HTTP headers collected");

  // HTTP GET
  start_time = millis();
  http_code = this->client_.GET();
  duration = millis() - start_time;
  ESP_LOGV(TAG, "HTTP GET finished");

  if (http_code >= 310) {
    ESP_LOGW(TAG, "HTTP Request failed; URL: %s; Error: %s (%d); Duration: %u ms", url,
             HTTPClient::errorToString(http_code).c_str(), http_code, duration);
    return -1;
  }

  if (this->client_.getSize() < 0) {
    ESP_LOGE(TAG, "Incorrect file size (%d) reported by HTTP server (status: %d). Aborting", this->client_.getSize(),
             http_code);
    return -1;
  }

  this->body_length_ = (size_t) this->client_.getSize();
  ESP_LOGV(TAG, "body_length: %d", this->body_length_);

#if defined(USE_ESP32) || defined(USE_RP2040)
  if (this->stream_ptr_ == nullptr) {
    this->set_stream_ptr_();
  }
#endif

  return this->body_length_;
}

int OtaHttpArduino::http_read(uint8_t *buf, const size_t max_len) {
  // wait for the stream to be populated
  while (this->stream_ptr_->available() == 0) {
    // give other tasks a chance to run while waiting for some data:
    // ESP_LOGVV(TAG, "not enougth data available: %zu (total read: %zu)", streamPtr->available(), bytes_read);
    App.feed_wdt();
    yield();
    delay(1);
  }
  int available_data = this->stream_ptr_->available();
  int bufsize = std::min((int) max_len, available_data);
  if (bufsize > 0) {
    // ESP_LOGVV(TAG, "data available: %zu", available_data);

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
