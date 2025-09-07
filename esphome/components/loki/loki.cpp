#include "loki.h"

#include <cinttypes>
#include "esphome/components/logger/logger.h"
#include "esphome/core/application.h"
#include "esphome/core/time.h"
#include "esphome/components/json/json_util.h"

namespace esphome {
namespace loki {

static const char *const TAG = "loki";

void Loki::setup() {
  logger::global_logger->add_on_log_callback(
      [this](int level, const char *tag, const char *message, size_t message_len) {
        this->log_(level, tag, message, message_len);
      });

  // Publish initial switch state
  if (this->logs_enabled_switch_ != nullptr) {
    this->logs_enabled_switch_->publish_state(this->enabled_);
  }
}

void Loki::log_(const int level, const char *tag, const char *message, size_t message_len) const {
  if (!this->enabled_ || level > this->log_level_)
    return;

  // Get current timestamp in nanoseconds
  auto now = this->time_->now();
  if (!now.is_valid()) {
    return;
  }
  // seconds -> nanoseconds as string
  int64_t ns = static_cast<int64_t>(now.timestamp) * 1000000000LL;
  char tsbuf[32];
  snprintf(tsbuf, sizeof(tsbuf), "%" PRId64, ns);

  size_t len = message_len;
  // remove color formatting
  if (this->strip_ && message[0] == 0x1B && len > 11) {
    message += 7;
    len -= 11;
  }

  // Create log message string
  std::string log_message(message, len);

  // Create Loki JSON payload
  std::string json_payload = json::build_json([&](JsonObject root) {
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
    JsonArray streams = root["streams"].to<JsonArray>();
    JsonObject entry = streams.add<JsonObject>();

    // Stream labels
    JsonObject labels = entry["stream"].to<JsonObject>();
    // add a platform tag to make it easy to fetch all logs
    labels["platform"] = "esphome";
    labels["job"] = "esphome";
    // Add node name
    labels["node"] = App.get_name();
    // Check for area and add if not empty
    const std::string &node_area = App.get_area();
    if (!node_area.empty()) {
      labels["area"] = node_area;
    }
    // Check for friendly_name and add if not empty
    const std::string &node_friendly_name = App.get_friendly_name();
    if (!node_friendly_name.empty()) {
      labels["friendly_name"] = node_friendly_name;
    }
    labels["tag"] = tag;
    labels["log_level"] = this->get_log_level_name_(level);

    // Log values (timestamp and message)
    JsonArray values = entry["values"].to<JsonArray>();
    JsonArray log_line = values.add<JsonArray>();
    log_line.add(tsbuf);
    log_line.add(log_message);
  });

  // Prepare headers
  std::list<http_request::Header> headers = {};
  headers.push_back({"Content-Type", "application/json"});

  // Construct full URL
  std::string full_url = this->url_;
  if (this->port_ != 80) {
    full_url += ":" + std::to_string(this->port_);
  }
  if (full_url.back() != '/') {
    full_url += "/";
  }
  full_url += "loki/api/v1/push";

  // Send HTTP POST request
  auto container = this->parent_->post(full_url, json_payload, headers);
  if (container == nullptr) {
    ESP_LOGE(TAG, "Failed to send log to Loki at %s", full_url.c_str());
  }
}

const char *Loki::get_log_level_name_(int level) const {
  switch (level) {
    case ESPHOME_LOG_LEVEL_ERROR:
      return "ERROR";
    case ESPHOME_LOG_LEVEL_WARN:
      return "WARN";
    case ESPHOME_LOG_LEVEL_INFO:
      return "INFO";
    case ESPHOME_LOG_LEVEL_DEBUG:
      return "DEBUG";
    case ESPHOME_LOG_LEVEL_VERBOSE:
      return "VERBOSE";
    case ESPHOME_LOG_LEVEL_VERY_VERBOSE:
      return "VERY_VERBOSE";
    default:
      return "UNKNOWN";
  }
}

}  // namespace loki
}  // namespace esphome
