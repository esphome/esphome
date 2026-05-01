#include "loop_test_isr_component.h"
#include "esphome/core/hal.h"
#include "esphome/core/application.h"

namespace esphome {
namespace loop_test_component {

static const char *const ISR_TAG = "loop_test_isr_component";

void LoopTestISRComponent::setup() {
  ESP_LOGI(ISR_TAG, "[%s] ISR component setup called", this->name_.c_str());
  this->last_check_time_ = millis();
}

void LoopTestISRComponent::loop() {
  this->loop_count_++;
  ESP_LOGI(ISR_TAG, "[%s] ISR component loop count: %d", this->name_.c_str(), this->loop_count_);

  // Disable after 5 loops
  if (this->loop_count_ == 5) {
    ESP_LOGI(ISR_TAG, "[%s] Disabling after 5 loops", this->name_.c_str());
    this->disable_loop();
    this->last_disable_time_ = millis();
    // Simulate ISR after disabling
    this->set_timeout("simulate_isr_1", 50, [this]() {
      ESP_LOGI(ISR_TAG, "[%s] Simulating ISR enable", this->name_.c_str());
      this->simulate_isr_enable();
      // Test reentrancy - call enable_loop() directly after ISR
      // This simulates another thread calling enable_loop while processing ISR enables
      this->set_timeout("test_reentrant", 10, [this]() {
        ESP_LOGI(ISR_TAG, "[%s] Testing reentrancy - calling enable_loop() directly", this->name_.c_str());
        this->enable_loop();
      });
    });
  }

  // If we get here after being disabled, it means ISR re-enabled us
  if (this->loop_count_ > 5 && this->loop_count_ < 10) {
    ESP_LOGI(ISR_TAG, "[%s] Running after ISR re-enable! ISR was called %d times", this->name_.c_str(),
             this->isr_call_count_);
  }

  // Disable again after 10 loops to test multiple ISR enables
  if (this->loop_count_ == 10) {
    ESP_LOGI(ISR_TAG, "[%s] Disabling again after 10 loops", this->name_.c_str());
    this->disable_loop();
    this->last_disable_time_ = millis();

    // Test pure ISR enable without any main loop enable
    this->set_timeout("simulate_isr_2", 50, [this]() {
      ESP_LOGI(ISR_TAG, "[%s] Testing pure ISR enable (no main loop enable)", this->name_.c_str());
      this->simulate_isr_enable();
      // DO NOT call enable_loop() - test that ISR alone works
    });
  }

  // Log when we're running after second ISR enable
  if (this->loop_count_ > 10) {
    ESP_LOGI(ISR_TAG, "[%s] Running after pure ISR re-enable! ISR was called %d times total", this->name_.c_str(),
             this->isr_call_count_);
  }
}

void IRAM_ATTR LoopTestISRComponent::simulate_isr_enable() {
  // This simulates what would happen in a real ISR
  // In a real scenario, this would be called from an actual interrupt handler

  this->isr_call_count_++;

  // Call enable_loop_soon_any_context multiple times to test that it's safe
  this->enable_loop_soon_any_context();
  this->enable_loop_soon_any_context();  // Test multiple calls
  this->enable_loop_soon_any_context();  // Should be idempotent

  // Note: In a real ISR, we cannot use ESP_LOG* macros as they're not ISR-safe
  // For testing, we'll track the call count and log it from the main loop
}

}  // namespace loop_test_component
}  // namespace esphome
