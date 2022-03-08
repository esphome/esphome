#include "http_request_arduino.h"

#ifdef USE_ARDUINO

#include "esphome/components/network/util.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

namespace esphome {
namespace http_request {

static const char *const TAG = "http_request.arduino";

void HttpRequestArduino::set_url(std::string url) {
  this->url_ = std::move(url);
  this->secure_ = this->url_.compare(0, 6, "https:") == 0;

  if (!this->last_url_.empty() && this->url_ != this->last_url_) {
    // Close connection if url has been changed
    this->client_.setReuse(false);
    this->client_.end();
  }
  this->client_.setReuse(true);
}

std::unique_ptr<HttpResponse> HttpRequestArduino::send() {
  if (!network::is_connected()) {
    this->client_.end();
    this->status_set_warning();
    ESP_LOGW(TAG, "HTTP Request failed; Not connected to network");
    return {-1, 0, {}};
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
    return {-1, 0, {}};
  }

  this->client_.setTimeout(this->timeout_);
  if (this->useragent_ != nullptr) {
    this->client_.setUserAgent(this->useragent_);
  }
  for (const auto &header : this->headers_) {
    this->client_.addHeader(header.name, header.value, false, true);
  }

  int http_code = this->client_.sendRequest(this->method_.c_str(), this->body_.c_str());

  std::unique_ptr<HttpResponse> response = make_unique<HttpResponse>();

  response->status_code = http_code;
  response->content_length = this->client_.getSize();

  if (http_code < 0) {
    ESP_LOGW(TAG, "HTTP Request failed; URL: %s; Error: %s", this->url_.c_str(),
             HTTPClient::errorToString(http_code).c_str());
    this->status_set_warning();
    return {http_code, 0, {}};
  }
  if (this->capture_response_) {
#ifdef USE_ESP32
    String str;
#else
    auto &
#endif
    str = this->client_.getString();
    response->data = std::vector<char>(str.c_str(), str.c_str() + str.length());
  }

  if (http_code < 200 || http_code >= 300) {
    ESP_LOGW(TAG, "HTTP Request failed; URL: %s; Code: %d", this->url_.c_str(), http_code);
    this->status_set_warning();
    return response;
  }

  this->status_clear_warning();
  ESP_LOGD(TAG, "HTTP Request completed; URL: %s; Code: %d", this->url_.c_str(), http_code);

  this->last_url_ = this->url_;
  this->client_.end();
  return response;
}

#ifdef USE_ESP8266
std::shared_ptr<WiFiClient> HttpRequestArduino::get_wifi_client_() {
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

}  // namespace http_request
}  // namespace esphome

#endif  // USE_ARDUINO
