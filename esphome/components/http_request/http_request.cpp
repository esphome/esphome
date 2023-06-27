#ifdef USE_ARDUINO

#include "http_request.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"

namespace esphome {
namespace http_request {

static const char *const TAG = "http_request";

void HttpRequestComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HTTP Request:");
  ESP_LOGCONFIG(TAG, "  Timeout: %ums", this->timeout_);
  ESP_LOGCONFIG(TAG, "  User-Agent: %s", this->useragent_);
  ESP_LOGCONFIG(TAG, "  Follow Redirects: %d", this->follow_redirects_);
  ESP_LOGCONFIG(TAG, "  Redirect limit: %d", this->redirect_limit_);
}

void HttpRequestComponent::set_url(std::string url) {
  this->url_ = std::move(url);
  this->secure_ = this->url_.compare(0, 6, "https:") == 0;

  if (!this->last_url_.empty() && this->url_ != this->last_url_) {
    // Close connection if url has been changed
    this->client_.setReuse(false);
    this->client_.end();
  }
  this->client_.setReuse(true);
}

void HttpRequestComponent::send(const std::vector<HttpRequestResponseTrigger *> &response_triggers) {
  if (!network::is_connected()) {
    this->client_.end();
    this->status_set_warning();
    ESP_LOGW(TAG, "HTTP Request failed; Not connected to network");
    return;
  }

  bool begin_status = false;
  const String url = this->url_.c_str();
#if defined(USE_ESP32) || (defined(USE_ESP8266) && USE_ARDUINO_VERSION_CODE >= VERSION_CODE(2, 6, 0))
#if defined(USE_ESP32) || USE_ARDUINO_VERSION_CODE >= VERSION_CODE(2, 7, 0)
  if (this->follow_redirects_) {
    this->client_.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  } else {
    this->client_.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  }
#else
  this->client_.setFollowRedirects(this->follow_redirects_);
#endif
  this->client_.setRedirectLimit(this->redirect_limit_);
#endif
#if defined(USE_ESP32)
  begin_status = this->client_.begin(url);
#elif defined(USE_ESP8266)
  begin_status = this->client_.begin(*this->get_wifi_client_(), url);
#endif

  if (!begin_status) {
    this->client_.end();
    this->status_set_warning();
    ESP_LOGW(TAG, "HTTP Request failed at the begin phase. Please check the configuration");
    return;
  }

  this->client_.setTimeout(this->timeout_);
#if defined(USE_ESP32)
  this->client_.setConnectTimeout(this->timeout_);
#endif
  if (this->useragent_ != nullptr) {
    this->client_.setUserAgent(this->useragent_);
  }
  for (const auto &header : this->headers_) {
    this->client_.addHeader(header.name, header.value, false, true);
  }

  uint32_t start_time = millis();
  int http_code = this->client_.sendRequest(this->method_, this->body_.c_str());
  uint32_t duration = millis() - start_time;
  for (auto *trigger : response_triggers)
    trigger->process(http_code, duration);

  if (http_code < 0) {
    ESP_LOGW(TAG, "HTTP Request failed; URL: %s; Error: %s; Duration: %u ms", this->url_.c_str(),
             HTTPClient::errorToString(http_code).c_str(), duration);
    this->status_set_warning();
    return;
  }

  if (http_code < 200 || http_code >= 300) {
    ESP_LOGW(TAG, "HTTP Request failed; URL: %s; Code: %d; Duration: %u ms", this->url_.c_str(), http_code, duration);
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();
  ESP_LOGD(TAG, "HTTP Request completed; URL: %s; Code: %d; Duration: %u ms", this->url_.c_str(), http_code, duration);
}

#ifdef USE_ESP8266
std::shared_ptr<WiFiClient> HttpRequestComponent::get_wifi_client_() {
#ifdef USE_HTTP_REQUEST_ESP8266_HTTPS
  if (this->secure_) {
    if (this->wifi_client_secure_ == nullptr) {
      this->wifi_client_secure_ = std::make_shared<BearSSL::WiFiClientSecure>();
      this->wifi_client_secure_->setInsecure();
      this->wifi_client_secure_->setBufferSizes(512, 512);
    }
    return this->wifi_client_secure_;
  }
#endif

  if (this->wifi_client_ == nullptr) {
    this->wifi_client_ = std::make_shared<WiFiClient>();
  }
  return this->wifi_client_;
}
#endif

void HttpRequestComponent::close() {
  this->last_url_ = this->url_;
  this->client_.end();
}

const char *HttpRequestComponent::get_string() {
#if defined(ESP32)
  // The static variable is here because HTTPClient::getString() returns a String on ESP32,
  // and we need something to keep a buffer alive.
  static String str;
#else
  // However on ESP8266, HTTPClient::getString() returns a String& to a member variable.
  // Leaving this the default so that any new platform either doesn't copy, or encounters a compilation error.
  auto &
#endif
  str = this->client_.getString();
  return str.c_str();
}

}  // namespace http_request
}  // namespace esphome

#endif  // USE_ARDUINO
