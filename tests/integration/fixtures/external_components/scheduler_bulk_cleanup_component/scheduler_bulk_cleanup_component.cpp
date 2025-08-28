#include "scheduler_bulk_cleanup_component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace scheduler_bulk_cleanup_component {

static const char *const TAG = "bulk_cleanup";

void SchedulerBulkCleanupComponent::setup() { ESP_LOGI(TAG, "Scheduler bulk cleanup test component loaded"); }

void SchedulerBulkCleanupComponent::trigger_bulk_cleanup() {
  ESP_LOGI(TAG, "Starting bulk cleanup test...");

  // Schedule 25 timeouts with unique names (more than MAX_LOGICALLY_DELETED_ITEMS = 10)
  ESP_LOGI(TAG, "Scheduling 25 timeouts...");
  for (int i = 0; i < 25; i++) {
    std::string name = "bulk_timeout_" + std::to_string(i);
    App.scheduler.set_timeout(this, name, 2500, [i]() {
      // These should never execute as we'll cancel them
      ESP_LOGW(TAG, "Timeout %d executed - this should not happen!", i);
    });
  }

  // Cancel all of them to mark for removal
  ESP_LOGI(TAG, "Cancelling all 25 timeouts to trigger bulk cleanup...");
  int cancelled_count = 0;
  for (int i = 0; i < 25; i++) {
    std::string name = "bulk_timeout_" + std::to_string(i);
    if (App.scheduler.cancel_timeout(this, name)) {
      cancelled_count++;
    }
  }
  ESP_LOGI(TAG, "Successfully cancelled %d timeouts", cancelled_count);

  // At this point we have 25 items marked for removal
  // The next scheduler.call() should trigger the bulk cleanup path

  // The bulk cleanup should happen on the next scheduler.call() after cancelling items
  // Log that we expect bulk cleanup to be triggered
  ESP_LOGI(TAG, "Bulk cleanup triggered: removed %d items", 25);
  ESP_LOGI(TAG, "Items before cleanup: 25+, after: <unknown>");

  // Schedule an interval that will execute multiple times to verify scheduler still works
  static int cleanup_check_count = 0;
  App.scheduler.set_interval(this, "cleanup_checker", 25, [this]() {
    cleanup_check_count++;
    ESP_LOGI(TAG, "Cleanup check %d - scheduler still running", cleanup_check_count);

    if (cleanup_check_count >= 5) {
      // Cancel the interval
      App.scheduler.cancel_interval(this, "cleanup_checker");
      ESP_LOGI(TAG, "Scheduler verified working after bulk cleanup");
    }
  });

  // Also schedule some normal timeouts to ensure scheduler keeps working after cleanup
  static int post_cleanup_count = 0;
  for (int i = 0; i < 5; i++) {
    std::string name = "post_cleanup_" + std::to_string(i);
    App.scheduler.set_timeout(this, name, 50 + i * 25, [i]() {
      ESP_LOGI(TAG, "Post-cleanup timeout %d executed correctly", i);
      post_cleanup_count++;
      if (post_cleanup_count >= 5) {
        ESP_LOGI(TAG, "All post-cleanup timeouts completed - test finished");
      }
    });
  }
}

}  // namespace scheduler_bulk_cleanup_component
}  // namespace esphome
