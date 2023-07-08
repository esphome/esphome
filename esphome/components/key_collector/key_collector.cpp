#include "key_collector.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace key_collector {

static const char *const TAG = "key_collector";

KeyCollector::KeyCollector()
    : progress_trigger_(new Trigger<std::string, uint8_t>()),
      result_trigger_(new Trigger<std::string, uint8_t, uint8_t>()),
      timeout_trigger_(new Trigger<std::string, uint8_t>()) {}

void KeyCollector::loop() {
  if ((this->timeout_ == 0) || this->result_.empty() || (millis() - this->last_key_time_ < this->timeout_))
    return;
  this->timeout_trigger_->trigger(this->result_, this->start_key_);
  this->clear();
}

void KeyCollector::dump_config() {
  ESP_LOGCONFIG(TAG, "Key Collector:");
  if (this->min_length_ > 0)
    ESP_LOGCONFIG(TAG, "  min length: %d", this->min_length_);
  if (this->max_length_ > 0)
    ESP_LOGCONFIG(TAG, "  max length: %d", this->max_length_);
  if (!this->back_keys_.empty())
    ESP_LOGCONFIG(TAG, "  erase keys '%s'", this->back_keys_.c_str());
  if (!this->clear_keys_.empty())
    ESP_LOGCONFIG(TAG, "  clear keys '%s'", this->clear_keys_.c_str());
  if (!this->start_keys_.empty())
    ESP_LOGCONFIG(TAG, "  start keys '%s'", this->start_keys_.c_str());
  if (!this->end_keys_.empty()) {
    ESP_LOGCONFIG(TAG, "  end keys '%s'", this->end_keys_.c_str());
    ESP_LOGCONFIG(TAG, "  end key is required: %s", ONOFF(this->end_key_required_));
  }
  if (!this->allowed_keys_.empty())
    ESP_LOGCONFIG(TAG, "  allowed keys '%s'", this->allowed_keys_.c_str());
  if (this->timeout_ > 0)
    ESP_LOGCONFIG(TAG, "  entry timeout: %0.1f", this->timeout_ / 1000.0);
}

void KeyCollector::set_provider(key_provider::KeyProvider *provider) {
  provider->add_on_key_callback([this](uint8_t key) { this->key_pressed_(key); });
}

void KeyCollector::clear(bool progress_update) {
  this->result_.clear();
  this->start_key_ = 0;
  if (progress_update)
    this->progress_trigger_->trigger(this->result_, 0);
}

void KeyCollector::send_key(uint8_t key) { this->key_pressed_(key); }

void KeyCollector::key_pressed_(uint8_t key) {
  this->last_key_time_ = millis();
  if (!this->start_keys_.empty() && !this->start_key_) {
    if (this->start_keys_.find(key) != std::string::npos) {
      this->start_key_ = key;
      this->progress_trigger_->trigger(this->result_, this->start_key_);
    }
    return;
  }
  if (this->back_keys_.find(key) != std::string::npos) {
    if (!this->result_.empty()) {
      this->result_.pop_back();
      this->progress_trigger_->trigger(this->result_, this->start_key_);
    }
    return;
  }
  if (this->clear_keys_.find(key) != std::string::npos) {
    if (!this->result_.empty())
      this->clear();
    return;
  }
  if (this->end_keys_.find(key) != std::string::npos) {
    if ((this->min_length_ == 0) || (this->result_.size() >= this->min_length_)) {
      this->result_trigger_->trigger(this->result_, this->start_key_, key);
      this->clear();
    }
    return;
  }
  if (!this->allowed_keys_.empty() && (this->allowed_keys_.find(key) == std::string::npos))
    return;
  if ((this->max_length_ == 0) || (this->result_.size() < this->max_length_))
    this->result_.push_back(key);
  if ((this->max_length_ > 0) && (this->result_.size() == this->max_length_) && (!this->end_key_required_)) {
    this->result_trigger_->trigger(this->result_, this->start_key_, 0);
    this->clear(false);
  }
  this->progress_trigger_->trigger(this->result_, this->start_key_);
}

}  // namespace key_collector
}  // namespace esphome
