#include "http_request_idf.h"

#ifdef USE_ESP_IDF

#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif

#include "watchdog.h"

namespace esphome {
namespace http_request {

static const char *const TAG = "http_request.idf";

std::shared_ptr<HttpContainer> HttpRequestIDF::start(std::string url, std::string method, std::string body,
                                                     std::list<Header> headers) {
  if (!network::is_connected()) {
    this->status_momentary_error("failed", 1000);
    ESP_LOGE(TAG, "HTTP Request failed; Not connected to network");
    return nullptr;
  }

  esp_http_client_method_t method_idf;
  if (method == "GET") {
    method_idf = HTTP_METHOD_GET;
  } else if (method == "POST") {
    method_idf = HTTP_METHOD_POST;
  } else if (method == "PUT") {
    method_idf = HTTP_METHOD_PUT;
  } else if (method == "DELETE") {
    method_idf = HTTP_METHOD_DELETE;
  } else if (method == "PATCH") {
    method_idf = HTTP_METHOD_PATCH;
  } else {
    this->status_momentary_error("failed", 1000);
    ESP_LOGE(TAG, "HTTP Request failed; Unsupported method");
    return nullptr;
  }

  bool secure = url.find("https:") != std::string::npos;

  esp_http_client_config_t config = {};

  config.url = url.c_str();
  config.method = method_idf;
  config.timeout_ms = this->timeout_;
  config.disable_auto_redirect = !this->follow_redirects_;
  config.max_redirection_count = this->redirect_limit_;
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
  if (secure) {
    config.crt_bundle_attach = esp_crt_bundle_attach;
  }
#endif

  if (this->useragent_ != nullptr) {
    config.user_agent = this->useragent_;
  }

  const uint32_t start = millis();
  watchdog::WatchdogManager wdm(this->get_watchdog_timeout());

  esp_http_client_handle_t client = esp_http_client_init(&config);

  std::shared_ptr<HttpContainerIDF> container = std::make_shared<HttpContainerIDF>(client);
  container->set_parent(this);

  container->set_secure(secure);

  for (const auto &header : headers) {
    esp_http_client_set_header(client, header.name, header.value);
  }

  int body_len = body.length();

  esp_err_t err = esp_http_client_open(client, body_len);
  if (err != ESP_OK) {
    this->status_momentary_error("failed", 1000);
    ESP_LOGE(TAG, "HTTP Request failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return nullptr;
  }

  if (body_len > 0) {
    int write_left = body_len;
    int write_index = 0;
    const char *buf = body.c_str();
    while (write_left > 0) {
      int written = esp_http_client_write(client, buf + write_index, write_left);
      if (written < 0) {
        err = ESP_FAIL;
        break;
      }
      write_left -= written;
      write_index += written;
    }
  }

  if (err != ESP_OK) {
    this->status_momentary_error("failed", 1000);
    ESP_LOGE(TAG, "HTTP Request failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return nullptr;
  }

  container->content_length = esp_http_client_fetch_headers(client);
  const auto status_code = esp_http_client_get_status_code(client);
  container->status_code = status_code;

  if (status_code < 200 || status_code >= 300) {
    ESP_LOGE(TAG, "HTTP Request failed; URL: %s; Code: %d", url.c_str(), status_code);
    this->status_momentary_error("failed", 1000);
    esp_http_client_cleanup(client);
    return nullptr;
  }
  container->duration_ms = millis() - start;
  return container;
}

int HttpContainerIDF::read(uint8_t *buf, size_t max_len) {
  const uint32_t start = millis();
  watchdog::WatchdogManager wdm(this->parent_->get_watchdog_timeout());

  int bufsize = std::min(max_len, this->content_length - this->bytes_read_);

  if (bufsize == 0) {
    this->duration_ms += (millis() - start);
    return 0;
  }

  App.feed_wdt();
  int read_len = esp_http_client_read(this->client_, (char *) buf, bufsize);
  this->bytes_read_ += read_len;

  this->duration_ms += (millis() - start);

  return read_len;
}

void HttpContainerIDF::end() {
  watchdog::WatchdogManager wdm(this->parent_->get_watchdog_timeout());

  esp_http_client_close(this->client_);
  esp_http_client_cleanup(this->client_);
}

}  // namespace http_request
}  // namespace esphome

#endif  // USE_ESP_IDF
