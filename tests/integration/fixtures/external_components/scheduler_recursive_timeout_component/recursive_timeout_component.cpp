#include "recursive_timeout_component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace scheduler_recursive_timeout_component {

static const char *const TAG = "scheduler_recursive_timeout";

void SchedulerRecursiveTimeoutComponent::setup() { ESP_LOGCONFIG(TAG, "SchedulerRecursiveTimeoutComponent setup"); }

void SchedulerRecursiveTimeoutComponent::run_recursive_timeout_test() {
  ESP_LOGI(TAG, "Starting recursive timeout test - scheduling timeout from within timeout");

  // Reset state
  this->nested_level_ = 0;

  // Schedule the initial timeout with 1ms delay
  this->set_timeout(1, [this]() {
    ESP_LOGI(TAG, "Executing initial timeout");
    this->nested_level_ = 1;

    // From within this timeout, schedule another timeout with 1ms delay
    this->set_timeout(1, [this]() {
      ESP_LOGI(TAG, "Executing nested timeout 1");
      this->nested_level_ = 2;

      // From within this nested timeout, schedule yet another timeout with 1ms delay
      this->set_timeout(1, [this]() {
        ESP_LOGI(TAG, "Executing nested timeout 2");
        this->nested_level_ = 3;

        // Test complete
        ESP_LOGI(TAG, "Recursive timeout test complete - all %d levels executed", this->nested_level_);
      });
    });
  });
}

}  // namespace scheduler_recursive_timeout_component
}  // namespace esphome
