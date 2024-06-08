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

}  // namespace http_request
}  // namespace esphome
