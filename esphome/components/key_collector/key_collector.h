#pragma once

#include <utility>
#include "esphome/components/key_provider/key_provider.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"

namespace esphome::key_collector {

class KeyCollector final : public Component {
 public:
  void loop() override;
  void dump_config() override;
  void add_provider(key_provider::KeyProvider *provider);
  void set_min_length(uint16_t min_length) { this->min_length_ = min_length; };
  void set_max_length(uint16_t max_length) { this->max_length_ = max_length; };
  // The key setters keep the pointer; the strings must outlive the component (codegen passes literals)
  void set_start_keys(const char *start_keys) { this->start_keys_ = StringRef(start_keys); };
  void set_end_keys(const char *end_keys) { this->end_keys_ = StringRef(end_keys); };
  void set_end_key_required(bool end_key_required) { this->end_key_required_ = end_key_required; };
  void set_back_keys(const char *back_keys) { this->back_keys_ = StringRef(back_keys); };
  void set_clear_keys(const char *clear_keys) { this->clear_keys_ = StringRef(clear_keys); };
  void set_allowed_keys(const char *allowed_keys) { this->allowed_keys_ = StringRef(allowed_keys); };
  template<typename F> void add_on_progress_callback(F &&callback) {
    this->progress_callbacks_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_result_callback(F &&callback) {
    this->result_callbacks_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_timeout_callback(F &&callback) {
    this->timeout_callbacks_.add(std::forward<F>(callback));
  }
  void set_timeout(int timeout) { this->timeout_ = timeout; };
  void set_enabled(bool enabled);

  void clear(bool progress_update = true);
  void send_key(uint8_t key);

 protected:
  uint16_t min_length_{0};
  uint16_t max_length_{0};
  StringRef start_keys_;
  StringRef end_keys_;
  bool end_key_required_{false};
  StringRef back_keys_;
  StringRef clear_keys_;
  StringRef allowed_keys_;
  std::string result_;
  uint8_t start_key_{0};
  LazyCallbackManager<void(const std::string &, uint8_t)> progress_callbacks_;
  LazyCallbackManager<void(const std::string &, uint8_t, uint8_t)> result_callbacks_;
  LazyCallbackManager<void(const std::string &, uint8_t)> timeout_callbacks_;
  uint32_t last_key_time_{};
  uint32_t timeout_{0};
  bool enabled_{};
};

}  // namespace esphome::key_collector
