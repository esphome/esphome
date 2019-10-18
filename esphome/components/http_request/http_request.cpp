#include "http_request.h"
#include "esphome/core/log.h"

namespace esphome {
namespace http_request {

static const char *TAG = "http_request";

void HttpRequestComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HTTP Request:");
  ESP_LOGCONFIG(TAG, "  URL: %s", this->url_);
  ESP_LOGCONFIG(TAG, "  Method: %s", this->method_);
  ESP_LOGCONFIG(TAG, "  Timeout: %dms", this->timeout_);
  if (this->fingerprint_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Using SSL Fingerprint");
  }
  if (this->useragent_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  User-Agent: %s", this->useragent_);
  }
  if (!this->headers_.empty()) {
    ESP_LOGCONFIG(TAG, "  Headers:");
    for (const auto &header : this->headers_) {
      ESP_LOGCONFIG(TAG, "    %s: %s", header.name, header.value);
    }
  }
}

void HttpRequestComponent::send() {
  bool begin_status;
#ifdef ARDUINO_ARCH_ESP32
  begin_status = this->client_.begin(this->url_);
#endif
#ifdef HTTP_CLIENT_SUPPORT_INSECURE
  begin_status = this->client_.begin(*this->wifi_client_, this->url_);
#else
#ifdef ARDUINO_ARCH_ESP8266
  if (this->fingerprint_ != nullptr)
    begin_status = this->client_.begin(this->url_, this->fingerprint_);
  else
    begin_status = this->client_.begin(this->url_);
#endif
#endif

  if (!begin_status) {
    this->client_.end();
    this->status_set_warning();
    ESP_LOGW(TAG, "HTTP Request failed at the begin phase. Please check the configuration");
    return;
  }

  this->client_.setTimeout(this->timeout_);
  if (this->useragent_ != nullptr) {
    this->client_.setUserAgent(this->useragent_);
  }
  for (const auto &header : this->headers_) {
    this->client_.addHeader(header.name, header.value, false, true);
  }

  int http_code = this->client_.sendRequest(this->method_, this->payload_.c_str());
  this->client_.end();

  if (http_code < 0) {
    ESP_LOGW(TAG, "HTTP Request failed; URL: %s; Error: %s", this->url_, HTTPClient::errorToString(http_code).c_str());
    this->status_set_warning();
    return;
  }

  if (http_code < 200 || http_code >= 300) {
    ESP_LOGW(TAG, "HTTP Request failed; URL: %s; Code: %d", this->url_, http_code);
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();
  ESP_LOGD(TAG, "HTTP Request completed; URL: %s; Code: %d", this->url_, http_code);
}

}  // namespace http_request
}  // namespace esphome
