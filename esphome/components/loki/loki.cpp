#include "loki.h"

#include <cinttypes>
#include "esphome/components/logger/logger.h"
#include "esphome/core/application.h"
#include "esphome/core/time.h"
#include "esphome/components/json/json_util.h"

namespace esphome::loki {

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

#ifdef USE_ESP32
  // No background task needed - HTTP requests processed in main loop
  // This prevents connection exhaustion issues
#endif
}

void Loki::log_(const int level, const char *tag, const char *message, size_t message_len) {
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

  // Send the payload via queue with request limiting
#ifdef USE_ESP32
  if (!this->enqueue_(json_payload.c_str(), json_payload.length())) {
    // Queue is full - increment counter but don't log immediately to avoid cascade effect
    this->loki_queue_.increment_dropped_count();
  }
#else
  this->send_to_loki_(json_payload);
#endif
}

void Loki::send_to_loki_(const std::string &json_payload) {
  // Direct send for all platforms - no queuing to prevent connection issues
  this->parent_->post(this->get_full_url_(), json_payload, this->get_headers_());
}

std::string Loki::get_full_url_() const {
  std::string full_url = this->url_;
  if (this->port_ != 80) {
    char port_buf[UINT32_MAX_STR_SIZE];
    uint32_to_str(port_buf, this->port_);
    full_url += ":";
    full_url += port_buf;
  }
  if (full_url.back() != '/') {
    full_url += "/";
  }
  full_url += "loki/api/v1/push";
  return full_url;
}

std::list<http_request::Header> Loki::get_headers_() const {
  std::list<http_request::Header> headers = {};
  headers.push_back({"Content-Type", "application/json"});
  return headers;
}

const char *Loki::get_log_level_name_(int level) const {
  switch (level) {
    case ESPHOME_LOG_LEVEL_ERROR:
      return "ERROR";
    case ESPHOME_LOG_LEVEL_WARN:
      return "WARN";
    case ESPHOME_LOG_LEVEL_INFO:
      return "INFO";
    case ESPHOME_LOG_LEVEL_CONFIG:
      return "CONFIG";
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

void Loki::loop() {
#ifdef USE_ESP32
  // Process queued HTTP requests with concurrency limiting
  uint32_t now = millis();

  // Only process if we have capacity and enough time has passed
  if (this->active_requests_ < MAX_CONCURRENT_REQUESTS && (now - this->last_request_time_) >= REQUEST_DELAY_MS) {
    struct QueueElement *elem = this->loki_queue_.pop();
    if (elem != nullptr) {
      if (this->enabled_) {
        // Set HTTP timeout to prevent hanging connections
        this->set_http_timeout_();

        // Send HTTP POST request directly
        std::string payload(elem->json_payload, elem->payload_len);
        this->parent_->post(this->get_full_url_(), payload, this->get_headers_());

        // Update request tracking
        this->active_requests_++;
        this->last_request_time_ = now;
      }
      this->loki_event_pool_.release(elem);
    }
  }

  // Decrement active requests periodically (HTTP requests complete)
  if (this->active_requests_ > 0 && (now - this->last_request_time_) > HTTP_TIMEOUT_MS) {
    this->active_requests_--;
    this->last_request_time_ = now;
  }
#else
  // No-op for non-ESP32 platforms since they use direct HTTP requests
#endif
}

#ifdef USE_ESP32
bool Loki::enqueue_(const char *json_payload, size_t len) {
  auto *elem = this->loki_event_pool_.allocate();

  if (!elem) {
    // Queue is full - increment counter but don't log immediately.
    // Logging here can cause a cascade effect: if Loki logging is enabled,
    // each dropped message would generate a log message, which could itself
    // be sent via Loki, causing more drops and more logs in a feedback loop
    // that eventually triggers a watchdog reset. Instead, we log periodically
    // in loop() to prevent blocking the event loop during spikes.
    this->loki_queue_.increment_dropped_count();
    return false;
  }

  // Use the helper to allocate and copy data
  if (!elem->set_data(json_payload, len)) {
    // Allocation failed, return elem to pool
    this->loki_event_pool_.release(elem);
    // Increment counter without logging to avoid cascade effect during memory pressure
    this->loki_queue_.increment_dropped_count();
    return false;
  }

  // Push to queue - always succeeds since we allocated from the pool
  this->loki_queue_.push(elem);
  return true;
}

void Loki::set_http_timeout_() {
  // Set HTTP timeout to prevent hanging connections
  this->parent_->set_timeout(HTTP_TIMEOUT_MS);
}
#endif  // USE_ESP32

}  // namespace esphome::loki
