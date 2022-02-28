#include "http_request.h"

#include "esphome/core/log.h"

namespace esphome {
namespace http_request {

static const char *const TAG = "http_request";

HttpRequestComponent::HttpRequestComponent() { global_http_request = this; }

void HttpRequestComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HTTP Request:");
  ESP_LOGCONFIG(TAG, "  Timeout: %ums", this->timeout_);
  ESP_LOGCONFIG(TAG, "  User-Agent: %s", this->useragent_);
  ESP_LOGCONFIG(TAG, "  Follow Redirects: %d", this->follow_redirects_);
  ESP_LOGCONFIG(TAG, "  Redirect limit: %d", this->redirect_limit_);
}

HttpRequestComponent *global_http_request;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace http_request
}  // namespace esphome
