#include "http_request_arduino.h"

#if defined(USE_ARDUINO) && !defined(USE_ESP32)

#include "esphome/components/network/util.h"
#include "esphome/components/watchdog/watchdog.h"

#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

namespace esphome::http_request {

static const char *const TAG = "http_request.arduino";

std::shared_ptr<HttpContainer> HttpRequestArduino::perform(const std::string &url, const std::string &method,
                                                           const std::string &body,
                                                           const std::list<Header> &request_headers,
                                                           const std::set<std::string> &collect_headers) {
  if (!network::is_connected()) {
    this->status_momentary_error("failed", 1000);
    ESP_LOGW(TAG, "HTTP Request failed; Not connected to network");
    return nullptr;
  }

  std::shared_ptr<HttpContainerArduino> container = std::make_shared<HttpContainerArduino>();
  container->set_parent(this);

  const uint32_t start = millis();

  bool secure = url.find("https:") != std::string::npos;
  container->set_secure(secure);

  watchdog::WatchdogManager wdm(this->get_watchdog_timeout());

  if (this->follow_redirects_) {
    container->client_.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    container->client_.setRedirectLimit(this->redirect_limit_);
  } else {
    container->client_.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  }

#if defined(USE_ESP8266)
  std::unique_ptr<WiFiClient> stream_ptr;
#ifdef USE_HTTP_REQUEST_ESP8266_HTTPS
  if (secure) {
    ESP_LOGV(TAG, "ESP8266 HTTPS connection with WiFiClientSecure");
    stream_ptr = std::make_unique<WiFiClientSecure>();
    WiFiClientSecure *secure_client = static_cast<WiFiClientSecure *>(stream_ptr.get());
    secure_client->setBufferSizes(512, 512);
    secure_client->setInsecure();
  } else {
    stream_ptr = std::make_unique<WiFiClient>();
  }
#else
  ESP_LOGV(TAG, "ESP8266 HTTP connection with WiFiClient");
  if (secure) {
    ESP_LOGE(TAG, "Can't use HTTPS connection with esp8266_disable_ssl_support");
    return nullptr;
  }
  stream_ptr = std::make_unique<WiFiClient>();
#endif  // USE_HTTP_REQUEST_ESP8266_HTTPS

#if USE_ARDUINO_VERSION_CODE >= VERSION_CODE(3, 1, 0)  // && USE_ARDUINO_VERSION_CODE < VERSION_CODE(?, ?, ?)
  if (!secure) {
    ESP_LOGW(TAG, "Using HTTP on Arduino version >= 3.1 is **very** slow. Consider setting framework version to 3.0.2 "
                  "in your YAML, or use HTTPS");
  }
#endif  // USE_ARDUINO_VERSION_CODE
  bool status = container->client_.begin(*stream_ptr, url.c_str());

#elif defined(USE_RP2040)
  if (secure) {
    container->client_.setInsecure();
  }
  bool status = container->client_.begin(url.c_str());
#endif

  App.feed_wdt();

  if (!status) {
    ESP_LOGW(TAG, "HTTP Request failed; URL: %s", url.c_str());
    container->end();
    this->status_momentary_error("failed", 1000);
    return nullptr;
  }

  container->client_.setReuse(true);
  container->client_.setTimeout(this->timeout_);

  if (this->useragent_ != nullptr) {
    container->client_.setUserAgent(this->useragent_);
  }
  for (const auto &header : request_headers) {
    container->client_.addHeader(header.name.c_str(), header.value.c_str(), false, true);
  }

  // returned needed headers must be collected before the requests
  const char *header_keys[collect_headers.size()];
  int index = 0;
  for (auto const &header_name : collect_headers) {
    header_keys[index++] = header_name.c_str();
  }
  container->client_.collectHeaders(header_keys, index);

  App.feed_wdt();
  container->status_code = container->client_.sendRequest(method.c_str(), body.c_str());
  App.feed_wdt();
  if (container->status_code < 0) {
    ESP_LOGW(TAG, "HTTP Request failed; URL: %s; Error: %s", url.c_str(),
             HTTPClient::errorToString(container->status_code).c_str());
    this->status_momentary_error("failed", 1000);
    container->end();
    return nullptr;
  }

  if (!is_success(container->status_code)) {
    ESP_LOGE(TAG, "HTTP Request failed; URL: %s; Code: %d", url.c_str(), container->status_code);
    this->status_momentary_error("failed", 1000);
    // Still return the container, so it can be used to get the status code and error message
  }

  container->response_headers_ = {};
  auto header_count = container->client_.headers();
  for (int i = 0; i < header_count; i++) {
    const std::string header_name = str_lower_case(container->client_.headerName(i).c_str());
    if (collect_headers.count(header_name) > 0) {
      std::string header_value = container->client_.header(i).c_str();
      ESP_LOGD(TAG, "Received response header, name: %s, value: %s", header_name.c_str(), header_value.c_str());
      container->response_headers_[header_name].push_back(header_value);
    }
  }

  // HTTPClient::getSize() returns -1 for chunked transfer encoding (no Content-Length).
  // When cast to size_t, -1 becomes SIZE_MAX (4294967295 on 32-bit).
  // The read() method handles this: bytes_read_ can never reach SIZE_MAX, so the
  // early return check (bytes_read_ >= content_length) will never trigger.
  int content_length = container->client_.getSize();
  ESP_LOGD(TAG, "Content-Length: %d", content_length);
  container->content_length = (size_t) content_length;
  container->duration_ms = millis() - start;

  return container;
}

// Arduino HTTP read implementation
//
// WARNING: Return values differ from BSD sockets! See http_request.h for full documentation.
//
// Arduino's WiFiClient is inherently non-blocking - available() returns 0 when
// no data is ready. We use connected() to distinguish "no data yet" from
// "connection closed".
//
// WiFiClient behavior:
//   available() > 0: data ready to read
//   available() == 0 && connected(): no data yet, still connected
//   available() == 0 && !connected(): connection closed
//
// We normalize to HttpContainer::read() contract (NOT BSD socket semantics!):
//   > 0: bytes read
//   0: no data yet, retry            <-- NOTE: 0 means retry, NOT EOF!
//   < 0: error/connection closed     <-- connection closed returns -1, not 0
int HttpContainerArduino::read(uint8_t *buf, size_t max_len) {
  const uint32_t start = millis();
  watchdog::WatchdogManager wdm(this->parent_->get_watchdog_timeout());

  WiFiClient *stream_ptr = this->client_.getStreamPtr();
  if (stream_ptr == nullptr) {
    ESP_LOGE(TAG, "Stream pointer vanished!");
    return HTTP_ERROR_CONNECTION_CLOSED;
  }

  int available_data = stream_ptr->available();
  // For chunked transfer encoding, HTTPClient::getSize() returns -1, which becomes SIZE_MAX when
  // cast to size_t. SIZE_MAX - bytes_read_ is still huge, so it won't limit the read.
  size_t remaining = (this->content_length > 0) ? (this->content_length - this->bytes_read_) : max_len;
  int bufsize = std::min(max_len, std::min(remaining, (size_t) available_data));

  if (bufsize == 0) {
    this->duration_ms += (millis() - start);
    // Check if we've read all expected content (only valid when content_length is known and not SIZE_MAX)
    // For chunked encoding (content_length == SIZE_MAX), we can't use this check
    if (this->content_length > 0 && this->bytes_read_ >= this->content_length) {
      return 0;  // All content read successfully
    }
    // No data available - check if connection is still open
    // For chunked encoding, !connected() after reading means EOF (all chunks received)
    // For known content_length with bytes_read_ < content_length, it means connection dropped
    if (!stream_ptr->connected()) {
      return HTTP_ERROR_CONNECTION_CLOSED;  // Connection closed or EOF for chunked
    }
    return 0;  // No data yet, caller should retry
  }

  App.feed_wdt();
  int read_len = stream_ptr->readBytes(buf, bufsize);
  this->bytes_read_ += read_len;

  this->duration_ms += (millis() - start);

  return read_len;
}

void HttpContainerArduino::end() {
  watchdog::WatchdogManager wdm(this->parent_->get_watchdog_timeout());
  this->client_.end();
}

}  // namespace esphome::http_request

#endif  // USE_ARDUINO && !USE_ESP32
