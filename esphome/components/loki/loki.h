#pragma once
#include <queue>
#include <string>
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/event_pool.h"
#include "esphome/core/lock_free_queue.h"
#include "esphome/components/http_request/http_request.h"
#include "esphome/components/time/real_time_clock.h"
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

#ifdef USE_NETWORK
namespace esphome {
namespace loki {

#ifdef USE_ESP32
struct QueueElement {
  char *json_payload;
  uint16_t payload_len;

  QueueElement() : json_payload(nullptr), payload_len(0) {}

  // Helper to set payload (uses RAMAllocator)
  bool set_data(const char *payload_data, size_t len) {
    // Check payload size limit
    if (len > std::numeric_limits<uint16_t>::max()) {
      return false;
    }

    // Use RAMAllocator with default flags (tries external RAM first, falls back to internal)
    RAMAllocator<char> allocator;

    if (payload_data && len) {
      json_payload = allocator.allocate(len);
      if (!json_payload) {
        return false;
      }
      memcpy(json_payload, payload_data, len);
      payload_len = static_cast<uint16_t>(len);
    } else {
      json_payload = nullptr;
      payload_len = 0;
    }
    return true;
  }

  // Helper to release (uses RAMAllocator)
  void release() {
    RAMAllocator<char> allocator;
    if (json_payload) {
      allocator.deallocate(json_payload, payload_len);
      json_payload = nullptr;
    }
    payload_len = 0;
  }
};
#endif

class Loki : public Component, public Parented<http_request::HttpRequestComponent> {
 public:
  Loki(int level, time::RealTimeClock *time) : log_level_(level), time_(time) {}
  void setup() override;
  void set_strip(bool strip) { this->strip_ = strip; }
  void set_url(const std::string &url) { this->url_ = url; }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_enabled(bool enabled) {
    this->enabled_ = enabled;
#ifdef USE_SWITCH
    if (this->logs_enabled_switch_ != nullptr) {
      this->logs_enabled_switch_->publish_state(enabled);
    }
#endif
  }

  // Helper methods
  bool is_enabled() const { return this->enabled_; }
  void enable() { this->set_enabled(true); }
  void disable() { this->set_enabled(false); }
  std::string get_url() const { return this->url_; }
  uint16_t get_port() const { return this->port_; }
  int get_log_level() const { return this->log_level_; }
  bool is_strip_enabled() const { return this->strip_; }
  void set_max_runs(int max_runs) { max_runs_ = max_runs; }
  void loop() override;

#ifdef USE_SWITCH
  // Switch methods
  void set_logs_enabled_switch(switch_::Switch *sw) { this->logs_enabled_switch_ = sw; }
#endif

 protected:
  int log_level_;
  void log_(int level, const char *tag, const char *message, size_t message_len) const;
  void send_to_loki_(const std::string &json_payload) const;
  std::string get_full_url_() const;
  std::list<http_request::Header> get_headers_() const;
  int num_runs_ = 0;
  int max_runs_ = 0;
  const char *get_log_level_name_(int level) const;
  time::RealTimeClock *time_;
  bool strip_{true};
  bool enabled_{true};
  std::string url_;
  uint16_t port_{3100};
#ifdef USE_SWITCH
  switch_::Switch *logs_enabled_switch_{nullptr};
#endif

#ifdef USE_ESP32
  static const uint8_t LOKI_QUEUE_LENGTH = 20;  // 20 queue elements for log messages
  static const size_t TASK_STACK_SIZE = 3072;
  static const ssize_t TASK_PRIORITY = 5;

  static void esphome_loki_task(void *params);
  EventPool<struct QueueElement, LOKI_QUEUE_LENGTH> loki_event_pool_;
  NotifyingLockFreeQueue<struct QueueElement, LOKI_QUEUE_LENGTH> loki_queue_;
  TaskHandle_t task_handle_{nullptr};
  bool enqueue_(const char *json_payload, size_t len) const;
#endif
};
}  // namespace loki
}  // namespace esphome
#endif
