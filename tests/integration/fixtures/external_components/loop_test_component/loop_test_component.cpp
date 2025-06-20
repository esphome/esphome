#include "loop_test_component.h"

namespace esphome {
namespace loop_test_component {

void LoopTestComponent::setup() { ESP_LOGI(TAG, "[%s] Setup called", this->name_.c_str()); }

void LoopTestComponent::loop() {
  this->loop_count_++;
  ESP_LOGI(TAG, "[%s] Loop count: %d", this->name_.c_str(), this->loop_count_);

  // Test self-disable after specified count
  if (this->disable_after_ > 0 && this->loop_count_ == this->disable_after_) {
    ESP_LOGI(TAG, "[%s] Disabling self after %d loops", this->name_.c_str(), this->disable_after_);
    this->disable_loop();
  }

  // Test redundant operations
  if (this->test_redundant_operations_ && this->loop_count_ == 5) {
    if (this->name_ == "redundant_enable") {
      ESP_LOGI(TAG, "[%s] Testing enable when already enabled", this->name_.c_str());
      this->enable_loop();
    } else if (this->name_ == "redundant_disable") {
      ESP_LOGI(TAG, "[%s] Testing disable when will be disabled", this->name_.c_str());
      // We'll disable at count 10, but try to disable again at 5
      this->disable_loop();
      ESP_LOGI(TAG, "[%s] First disable complete", this->name_.c_str());
    }
  }
}

void LoopTestComponent::service_enable() {
  ESP_LOGI(TAG, "[%s] Service enable called", this->name_.c_str());
  this->enable_loop();
}

void LoopTestComponent::service_disable() {
  ESP_LOGI(TAG, "[%s] Service disable called", this->name_.c_str());
  this->disable_loop();
}

}  // namespace loop_test_component
}  // namespace esphome
