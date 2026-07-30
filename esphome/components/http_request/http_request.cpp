#include "http_request.h"

#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome::http_request {

static const char *const TAG = "http_request";

void HttpRequestComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "HTTP Request:\n"
                "  Timeout: %" PRIu32 "ms\n"
                "  User-Agent: %s\n"
                "  Follow redirects: %s\n"
                "  Redirect limit: %d",
                this->timeout_, this->useragent_, YESNO(this->follow_redirects_), this->redirect_limit_);
  if (this->watchdog_timeout_ > 0) {
    ESP_LOGCONFIG(TAG, "  Watchdog Timeout: %" PRIu32 "ms", this->watchdog_timeout_);
  }
}

std::string HttpContainer::get_response_header(const std::string &header_name) {
  auto lower = str_lower_case(header_name);  // NOLINT
  for (const auto &entry : this->response_headers_) {
    if (entry.name == lower) {
      ESP_LOGD(TAG, "Header with name %s found with value %s", lower.c_str(), entry.value.c_str());
      return entry.value;
    }
  }
  ESP_LOGW(TAG, "No header with name %s found", lower.c_str());
  return "";
}

}  // namespace esphome::http_request
