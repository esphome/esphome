#include "http_request_host.h"

#ifdef USE_HOST

#include <regex>
#include "esphome/components/network/util.h"
#include "esphome/components/watchdog/watchdog.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace http_request {

static const char *const TAG = "http_request.host";

std::shared_ptr<HttpContainer> HttpRequestHost::perform(std::string url, std::string method, std::string body,
                                                        std::list<Header> request_headers,
                                                        std::set<std::string> response_headers) {
  if (!network::is_connected()) {
    this->status_momentary_error("failed", 1000);
    ESP_LOGW(TAG, "HTTP Request failed; Not connected to network");
    return nullptr;
  }

  std::regex url_regex(R"(^(([^:\/?#]+):)?(//([^\/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?)", std::regex::extended);
  std::smatch url_match_result;

  if (!std::regex_match(url, url_match_result, url_regex) || url_match_result.length() < 7) {
    ESP_LOGE(TAG, "HTTP Request failed; Malformed URL: %s", url.c_str());
    return nullptr;
  }
  auto host = url_match_result[4].str();
  auto scheme_host = url_match_result[1].str() + url_match_result[3].str();
  auto path = url_match_result[5].str() + url_match_result[6].str();
  if (path.empty())
    path = "/";

  std::shared_ptr<HttpContainerHost> container = std::make_shared<HttpContainerHost>();
  container->set_parent(this);

  const uint32_t start = millis();

  watchdog::WatchdogManager wdm(this->get_watchdog_timeout());

  httplib::Headers h_headers;
  h_headers.emplace("Host", host.c_str());
  h_headers.emplace("User-Agent", this->useragent_);
  for (const auto &[name, value] : request_headers) {
    h_headers.emplace(name, value);
  }
  httplib::Client client(scheme_host.c_str());
  if (!client.is_valid()) {
    ESP_LOGE(TAG, "HTTP Request failed; Invalid URL: %s", url.c_str());
    return nullptr;
  }
  client.set_follow_location(this->follow_redirects_);
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
  if (this->ca_path_ != nullptr)
    client.set_ca_cert_path(this->ca_path_);
#endif

  httplib::Result result;
  if (method == "GET") {
    result = client.Get(path, h_headers, [&](const char *data, size_t data_length) {
      ESP_LOGV(TAG, "Got data length: %zu", data_length);
      container->response_body_.insert(container->response_body_.end(), (const uint8_t *) data,
                                       (const uint8_t *) data + data_length);
      return true;
    });
  } else if (method == "HEAD") {
    result = client.Head(path, h_headers);
  } else if (method == "PUT") {
    result = client.Put(path, h_headers, body, "");
    if (result) {
      auto data = std::vector<uint8_t>(result->body.begin(), result->body.end());
      container->response_body_.insert(container->response_body_.end(), data.begin(), data.end());
    }
  } else if (method == "PATCH") {
    result = client.Patch(path, h_headers, body, "");
    if (result) {
      auto data = std::vector<uint8_t>(result->body.begin(), result->body.end());
      container->response_body_.insert(container->response_body_.end(), data.begin(), data.end());
    }
  } else if (method == "POST") {
    result = client.Post(path, h_headers, body, "");
    if (result) {
      auto data = std::vector<uint8_t>(result->body.begin(), result->body.end());
      container->response_body_.insert(container->response_body_.end(), data.begin(), data.end());
    }
  } else {
    ESP_LOGW(TAG, "HTTP Request failed - unsupported method %s; URL: %s", method.c_str(), url.c_str());
    container->end();
    return nullptr;
  }
  App.feed_wdt();
  if (!result) {
    ESP_LOGW(TAG, "HTTP Request failed; URL: %s, error code: %u", url.c_str(), (unsigned) result.error());
    container->end();
    this->status_momentary_error("failed", 1000);
    return nullptr;
  }
  App.feed_wdt();
  auto response = *result;
  container->status_code = response.status;
  if (!is_success(response.status)) {
    ESP_LOGE(TAG, "HTTP Request failed; URL: %s; Code: %d", url.c_str(), response.status);
    this->status_momentary_error("failed", 1000);
    // Still return the container, so it can be used to get the status code and error message
  }

  container->content_length = container->response_body_.size();
  for (auto header : response.headers) {
    ESP_LOGD(TAG, "Header: %s: %s", header.first.c_str(), header.second.c_str());
    auto lower_name = str_lower_case(header.first);
    if (response_headers.find(lower_name) != response_headers.end()) {
      container->response_headers_[lower_name].emplace_back(header.second);
    }
  }
  container->duration_ms = millis() - start;
  return container;
}

int HttpContainerHost::read(uint8_t *buf, size_t max_len) {
  auto bytes_remaining = this->response_body_.size() - this->bytes_read_;
  auto read_len = std::min(max_len, bytes_remaining);
  memcpy(buf, this->response_body_.data() + this->bytes_read_, read_len);
  this->bytes_read_ += read_len;
  return read_len;
}

void HttpContainerHost::end() {
  watchdog::WatchdogManager wdm(this->parent_->get_watchdog_timeout());
  this->response_body_ = std::vector<uint8_t>();
  this->bytes_read_ = 0;
}

}  // namespace http_request
}  // namespace esphome

#endif  // USE_HOST
