#include "http_request_idf.h"

#ifdef USE_ESP_IDF

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
          auto &response = *reinterpret_cast<HttpResponse *>(evt->user_data);
          auto *const data_begin = reinterpret_cast<char *>(evt->data);
          auto *const data_end = data_begin + evt->data_len;
          response.data.insert(response.data.end(), data_begin, data_end);
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

std::unique_ptr<HttpResponse> HttpRequestIDF::send() {
  if (!network::is_connected()) {
    this->status_set_warning();
    ESP_LOGE(TAG, "HTTP Request failed; Not connected to network");
    return nullptr;
  }

  esp_http_client_method_t method;
  if (this->method_ == "GET") {
    method = HTTP_METHOD_GET;
  } else if (this->method_ == "POST") {
    method = HTTP_METHOD_POST;
  } else if (this->method_ == "PUT") {
    method = HTTP_METHOD_PUT;
  } else if (this->method_ == "DELETE") {
    method = HTTP_METHOD_DELETE;
  } else if (this->method_ == "PATCH") {
    method = HTTP_METHOD_PATCH;
  } else {
    this->status_set_warning();
    ESP_LOGE(TAG, "HTTP Request failed; Unsupported method");
    return nullptr;
  }

  HttpResponse response = {};  // used as user_data, by http_event_handler, in esp_http_client_perform
  esp_http_client_config_t config = {};

  config.url = this->url_.c_str();
  config.method = method;
  config.timeout_ms = this->timeout_;
  config.disable_auto_redirect = !this->follow_redirects_;
  config.max_redirection_count = this->redirect_limit_;
  config.user_data = reinterpret_cast<void *>(&response);
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
    return nullptr;
  }

  const auto status_code = esp_http_client_get_status_code(client);
  response.status_code = status_code;
  response.content_length = esp_http_client_get_content_length(client);

  if (status_code < 200 || status_code >= 300) {
    ESP_LOGE(TAG, "HTTP Request failed; URL: %s; Code: %d", this->url_.c_str(), status_code);
    this->status_set_warning();
    return make_unique<HttpResponse>(std::move(response));
  }

  this->status_clear_warning();
  ESP_LOGD(TAG, "HTTP Request completed; URL: %s; Code: %d", this->url_.c_str(), status_code);

  esp_http_client_cleanup(client);

  return make_unique<HttpResponse>(std::move(response));
}

}  // namespace http_request
}  // namespace esphome

#endif  // USE_ESP_IDF
