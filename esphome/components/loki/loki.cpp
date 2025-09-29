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

#ifdef USE_ESP32
  // Create the background task for processing Loki messages
  xTaskCreate(esphome_loki_task, "esphome_loki", TASK_STACK_SIZE, (void *) this, TASK_PRIORITY, &this->task_handle_);
  if (this->task_handle_ == nullptr) {
    // Don't log here - would cause infinite recursion since we're in the logger callback chain
    return;
  }
  // Set the task handle so the queue can notify it
  this->loki_queue_.set_task_to_notify(this->task_handle_);
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

  // Send the payload
#ifdef USE_ESP32
  // Add to batch instead of sending immediately
  this->add_to_batch_(level, tag, message, len, ns);
#else
  this->send_to_loki_(json_payload);
#endif
}

void Loki::send_to_loki_(const std::string &json_payload) {
#ifdef USE_ESP32
  // Use queue for non-blocking operation on ESP32
  if (!this->enqueue_(json_payload.c_str(), json_payload.length())) {
    // Queue is full - increment counter but don't log immediately to avoid cascade effect
    this->loki_queue_.increment_dropped_count();
  }
#else
  // Direct send for non-ESP32 platforms
  this->parent_->post(this->get_full_url_(), json_payload, this->get_headers_());
#endif
}

std::string Loki::get_full_url_() const {
  std::string full_url = this->url_;
  if (this->port_ != 80) {
    full_url += ":" + std::to_string(this->port_);
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
  // Periodically check for dropped messages to avoid blocking during spikes.
  // During high load, many messages can be dropped in quick succession.
  // We don't log dropped messages here to avoid infinite recursion since
  // logging would trigger the Loki logger callback, which could cause more drops.
  // Instead, we just reset the counter to prevent it from growing indefinitely.
  uint16_t dropped_count = this->loki_queue_.get_and_reset_dropped_count();
  // Silently handle dropped messages - no logging to prevent recursion
  (void) dropped_count;  // Suppress unused variable warning

  // Check if we need to send a batch due to timeout
  uint32_t now = millis();
  if (!this->log_batch_.empty() && (now - this->last_batch_time_) >= BATCH_TIMEOUT_MS) {
    this->send_batch_();
  }
#else
  // No-op for non-ESP32 platforms since they use direct HTTP requests
#endif
}

#ifdef USE_ESP32
void Loki::esphome_loki_task(void *params) {
  Loki *this_loki = (Loki *) params;

  while (true) {
    // Wait for notification indefinitely
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Process only one queued item per iteration to prevent blocking
    struct QueueElement *elem = this_loki->loki_queue_.pop();
    if (elem != nullptr) {
      if (this_loki->enabled_) {
        // Set HTTP timeout to prevent hanging connections
        this_loki->set_http_timeout_();

        // Send HTTP POST request directly
        std::string payload(elem->json_payload, elem->payload_len);
        this_loki->parent_->post(this_loki->get_full_url_(), payload, this_loki->get_headers_());
      }
      this_loki->loki_event_pool_.release(elem);
    }
  }

  // Note: This task runs indefinitely until the device reboots
  // The EventPool destructor will clean up the pool when the component is destroyed
  // No need for explicit cleanup since the task never exits
}

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

void Loki::add_to_batch_(int level, const char *tag, const char *message, size_t message_len, int64_t timestamp_ns) {
  // Remove color formatting if enabled
  size_t len = message_len;
  const char *msg = message;
  if (this->strip_ && message[0] == 0x1B && len > 11) {
    msg += 7;
    len -= 11;
  }

  // Add to batch
  this->log_batch_.emplace_back(level, tag, msg, len, timestamp_ns);
  this->last_batch_time_ = millis();

  // Send batch if it's full
  if (this->log_batch_.size() >= BATCH_SIZE) {
    this->send_batch_();
  }
}

void Loki::send_batch_() {
  if (this->log_batch_.empty()) {
    return;
  }

  // Rate limit HTTP requests to prevent connection exhaustion
  uint32_t now = millis();
  if (now - this->last_http_time_ < CONNECTION_DELAY_MS) {
    // Too soon since last request, skip this batch
    return;
  }

  // Build JSON payload for the batch
  std::string json_payload = this->build_batch_json_();

  // Send via queue
  if (!this->enqueue_(json_payload.c_str(), json_payload.length())) {
    // Queue is full - increment counter but don't log immediately to avoid cascade effect
    this->loki_queue_.increment_dropped_count();
  }

  // Update timing
  this->last_http_time_ = now;

  // Clear the batch
  this->log_batch_.clear();
}

std::string Loki::build_batch_json_() {
  return json::build_json([&](JsonObject root) {
    JsonArray streams = root["streams"].to<JsonArray>();

    // Group log entries by tag and level for efficiency
    std::map<std::string, std::vector<BatchedLogEntry *>> grouped_logs;
    for (auto &entry : this->log_batch_) {
      std::string key = entry.tag + ":" + this->get_log_level_name_(entry.level);
      grouped_logs[key].push_back(&entry);
    }

    // Create one stream per tag/level combination
    for (auto &[key, entries] : grouped_logs) {
      JsonObject stream_entry = streams.add<JsonObject>();

      // Stream labels
      JsonObject labels = stream_entry["stream"].to<JsonObject>();
      labels["platform"] = "esphome";
      labels["job"] = "esphome";
      labels["node"] = App.get_name();

      // Add area if available
      const std::string &node_area = App.get_area();
      if (!node_area.empty()) {
        labels["area"] = node_area;
      }

      // Add friendly_name if available
      const std::string &node_friendly_name = App.get_friendly_name();
      if (!node_friendly_name.empty()) {
        labels["friendly_name"] = node_friendly_name;
      }

      // Add tag and level from first entry (all entries in this group have same tag/level)
      labels["tag"] = entries[0]->tag;
      labels["log_level"] = this->get_log_level_name_(entries[0]->level);

      // Log values (timestamp and message pairs)
      JsonArray values = stream_entry["values"].to<JsonArray>();
      for (auto *entry : entries) {
        JsonArray log_line = values.add<JsonArray>();
        log_line.add(std::to_string(entry->timestamp_ns));
        log_line.add(entry->message);
      }
    }
  });
}

void Loki::set_http_timeout_() {
  // Set HTTP timeout to prevent hanging connections
  this->parent_->set_timeout(HTTP_TIMEOUT_MS);
}
#endif  // USE_ESP32

}  // namespace loki
}  // namespace esphome
