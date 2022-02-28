#include "http_request_idf.h"

#ifdef USE_ESP32

#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

namespace esphome {
namespace http_request {

static const char *const TAG = "http_request.idf";

esp_err_t http_event_handler(esp_http_client_event_t *evt) {
  App.feed_wdt();
  switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
      ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
      break;
    case HTTP_EVENT_ON_CONNECTED:
      ESP_LOGV(TAG, "HTTP_EVENT_ON_CONNECTED");
      break;
    case HTTP_EVENT_HEADER_SENT:
      ESP_LOGV(TAG, "HTTP_EVENT_HEADER_SENT");
      break;
    case HTTP_EVENT_ON_HEADER:
      ESP_LOGV(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
      break;
    case HTTP_EVENT_ON_DATA:
      ESP_LOGV(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
      /*
       *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
       *  However, event handler can also be used in case chunked encoding is used.
       */
      if (!esp_http_client_is_chunked_response(evt->client)) {
        if (global_http_request->get_capture_response()) {
          std::vector<char> &buffer = *reinterpret_cast<std::vector<char> *>(evt->user_data);
          buffer.insert(buffer.end(), reinterpret_cast<char *>(evt->data),
                        reinterpret_cast<char *>(evt->data) + evt->data_len);
        }
      }

      break;

    case HTTP_EVENT_ON_FINISH:
      ESP_LOGV(TAG, "HTTP_EVENT_ON_FINISH");
      break;
    case HTTP_EVENT_DISCONNECTED:
      ESP_LOGV(TAG, "HTTP_EVENT_DISCONNECTED");
      break;
  }
  return ESP_OK;
}

void HttpRequestIDF::set_url(std::string url) { this->url_ = std::move(url); }

HttpResponse HttpRequestIDF::send(bool capture_response) {
  if (!network::is_connected()) {
    this->status_set_warning();
    ESP_LOGE(TAG, "HTTP Request failed; Not connected to network");
    return {-1, 0, {}};
  }

  std::vector<char> buffer;

  esp_http_client_method_t method;
  if (this->method_ == "GET")
    method = HTTP_METHOD_GET;
  else if (this->method_ == "POST")
    method = HTTP_METHOD_POST;
  else if (this->method_ == "PUT")
    method = HTTP_METHOD_PUT;
  else if (this->method_ == "DELETE")
    method = HTTP_METHOD_DELETE;
  else if (this->method_ == "PATCH")
    method = HTTP_METHOD_PATCH;
  else
    method = HTTP_METHOD_GET;

  esp_http_client_config_t config = {};

  config.url = this->url_.c_str();
  config.method = method;
  config.timeout_ms = this->timeout_;
  config.disable_auto_redirect = !this->follow_redirects_;
  config.max_redirection_count = this->redirect_limit_;
  config.user_data = (void *) &buffer;
  config.event_handler = &http_event_handler;

  if (this->useragent_ != nullptr) {
    config.user_agent = this->useragent_;
  }

  esp_http_client_handle_t client = esp_http_client_init(&config);

  for (const auto &header : this->headers_) {
    esp_http_client_set_header(client, header.name, header.value);
  }

  if (!this->body_.empty()) {
    esp_http_client_set_post_field(client, this->body_.c_str(), this->body_.length());
  }

  esp_err_t err = esp_http_client_perform(client);

  if (err != ESP_OK) {
    this->status_set_warning();
    ESP_LOGE(TAG, "HTTP Request failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return {-1, 0, {}};
  }

  HttpResponse response = {};

  response.status_code = esp_http_client_get_status_code(client);
  response.content_length = esp_http_client_get_content_length(client);
  response.data = std::move(buffer);

  if (response.status_code < 200 || response.status_code >= 300) {
    ESP_LOGE(TAG, "HTTP Request failed; URL: %s; Code: %d", this->url_.c_str(), response.status_code);
    this->status_set_warning();
    return response;
  }

  this->status_clear_warning();
  ESP_LOGD(TAG, "HTTP Request completed; URL: %s; Code: %d", this->url_.c_str(), response.status_code);

  esp_http_client_cleanup(client);
  return response;
}

}  // namespace http_request
}  // namespace esphome

#endif  // USE_ESP32
