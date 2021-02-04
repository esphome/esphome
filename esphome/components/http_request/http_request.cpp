#include "http_request.h"
#include "esphome/core/log.h"

namespace esphome {
namespace http_request {

static const char *TAG = "http_request";

void HttpRequestComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HTTP Request:");
  ESP_LOGCONFIG(TAG, "  Timeout: %ums", this->timeout_);
  ESP_LOGCONFIG(TAG, "  User-Agent: %s", this->useragent_);
}

void HttpRequestComponent::set_url(std::string url) {
  this->url_ = url;
  this->secure_ = url.compare(0, 6, "https:") == 0;

  if (!this->last_url_.empty() && this->url_ != this->last_url_) {
    // Close connection if url has been changed
    this->client_.setReuse(false);
    this->client_.end();
  }
  this->client_.setReuse(true);
}

void HttpRequestComponent::send() {
  bool begin_status = false;
  const String url = this->url_.c_str();
#ifdef ARDUINO_ARCH_ESP32
  begin_status = this->client_.begin(url);
#endif
#ifdef ARDUINO_ARCH_ESP8266
#ifndef CLANG_TIDY
  this->client_.setFollowRedirects(true);
  this->client_.setRedirectLimit(3);
  begin_status = this->client_.begin(*this->get_wifi_client_(), url);
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

  int http_code = this->client_.sendRequest(this->method_, this->body_.c_str());
  if (http_code < 0) {
    ESP_LOGW(TAG, "HTTP Request failed; URL: %s; Error: %s", this->url_.c_str(),
             HTTPClient::errorToString(http_code).c_str());
    this->status_set_warning();
    return;
  }

  if (http_code < 200 || http_code >= 300) {
    ESP_LOGW(TAG, "HTTP Request failed; URL: %s; Code: %d", this->url_.c_str(), http_code);
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();
  ESP_LOGD(TAG, "HTTP Request completed; URL: %s; Code: %d", this->url_.c_str(), http_code);
}

#ifdef ARDUINO_ARCH_ESP8266
WiFiClient *HttpRequestComponent::get_wifi_client_() {
  if (this->secure_) {
    if (this->wifi_client_secure_ == nullptr) {
      this->wifi_client_secure_ = new BearSSL::WiFiClientSecure();
      this->wifi_client_secure_->setInsecure();
      this->wifi_client_secure_->setBufferSizes(512, 512);
    }
    return this->wifi_client_secure_;
  }

  if (this->wifi_client_ == nullptr) {
    this->wifi_client_ = new WiFiClient();
  }
  return this->wifi_client_;
}
#endif

void HttpRequestComponent::close() {
  this->last_url_ = this->url_;
  this->client_.end();
}

const char *HttpRequestComponent::get_string() {
  static const String STR = this->client_.getString();
  return STR.c_str();
}

}  // namespace http_request
}  // namespace esphome
