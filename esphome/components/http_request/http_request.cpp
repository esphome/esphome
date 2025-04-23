#include "http_request.h"

#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace http_request {

static const char *const TAG = "http_request";

void HttpRequestComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HTTP Request:");
  ESP_LOGCONFIG(TAG, "  Timeout: %ums", this->timeout_);
  ESP_LOGCONFIG(TAG, "  User-Agent: %s", this->useragent_);
  ESP_LOGCONFIG(TAG, "  Follow redirects: %s", YESNO(this->follow_redirects_));
  ESP_LOGCONFIG(TAG, "  Redirect limit: %d", this->redirect_limit_);
  if (this->watchdog_timeout_ > 0) {
    ESP_LOGCONFIG(TAG, "  Watchdog Timeout: %" PRIu32 "ms", this->watchdog_timeout_);
  }
}

std::string HttpContainer::get_response_header(const std::string &header_name) {
  auto response_headers = this->get_response_headers();
  auto header_name_lower_case = str_lower_case(header_name);
  if (response_headers.count(header_name_lower_case) == 0) {
    ESP_LOGW(TAG, "No header with name %s found", header_name_lower_case.c_str());
    return "";
  } else {
    auto values = response_headers[header_name_lower_case];
    if (values.empty()) {
      ESP_LOGE(TAG, "header with name %s returned an empty list, this shouldn't happen",
               header_name_lower_case.c_str());
      return "";
    } else {
      auto header_value = values.front();
      ESP_LOGD(TAG, "Header with name %s found with value %s", header_name_lower_case.c_str(), header_value.c_str());
      return header_value;
    }
  }
}

}  // namespace http_request
}  // namespace esphome
