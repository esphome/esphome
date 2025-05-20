#pragma once

#include <utility>
#include "esphome/components/key_provider/key_provider.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace key_collector {

class KeyCollector : public Component {
 public:
  KeyCollector();
  void loop() override;
  void dump_config() override;
  void set_provider(key_provider::KeyProvider *provider);
  void set_min_length(int min_length) { this->min_length_ = min_length; };
  void set_max_length(int max_length) { this->max_length_ = max_length; };
  void set_start_keys(std::string start_keys) { this->start_keys_ = std::move(start_keys); };
  void set_end_keys(std::string end_keys) { this->end_keys_ = std::move(end_keys); };
  void set_end_key_required(bool end_key_required) { this->end_key_required_ = end_key_required; };
  void set_back_keys(std::string back_keys) { this->back_keys_ = std::move(back_keys); };
  void set_clear_keys(std::string clear_keys) { this->clear_keys_ = std::move(clear_keys); };
  void set_allowed_keys(std::string allowed_keys) { this->allowed_keys_ = std::move(allowed_keys); };
  Trigger<std::string, uint8_t> *get_progress_trigger() const { return this->progress_trigger_; };
  Trigger<std::string, uint8_t, uint8_t> *get_result_trigger() const { return this->result_trigger_; };
  Trigger<std::string, uint8_t> *get_timeout_trigger() const { return this->timeout_trigger_; };
  void set_timeout(int timeout) { this->timeout_ = timeout; };
  void set_enabled(bool enabled);

  void clear(bool progress_update = true);
  void send_key(uint8_t key);

 protected:
  void key_pressed_(uint8_t key);

  int min_length_{0};
  int max_length_{0};
  std::string start_keys_;
  std::string end_keys_;
  bool end_key_required_{false};
  std::string back_keys_;
  std::string clear_keys_;
  std::string allowed_keys_;
  std::string result_;
  uint8_t start_key_{0};
  Trigger<std::string, uint8_t> *progress_trigger_;
  Trigger<std::string, uint8_t, uint8_t> *result_trigger_;
  Trigger<std::string, uint8_t> *timeout_trigger_;
  uint32_t last_key_time_;
  uint32_t timeout_{0};
  bool enabled_;
};

template<typename... Ts> class EnableAction : public Action<Ts...>, public Parented<KeyCollector> {
  void play(Ts... x) override { this->parent_->set_enabled(true); }
};

template<typename... Ts> class DisableAction : public Action<Ts...>, public Parented<KeyCollector> {
  void play(Ts... x) override { this->parent_->set_enabled(false); }
};

}  // namespace key_collector
}  // namespace esphome
