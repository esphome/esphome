#include "rapid_cancellation_component.h"
#include "esphome/core/log.h"
#include <thread>
#include <vector>
#include <chrono>
#include <random>
#include <sstream>

namespace esphome {
namespace scheduler_rapid_cancellation_component {

static const char *const TAG = "scheduler_rapid_cancellation";

void SchedulerRapidCancellationComponent::setup() { ESP_LOGCONFIG(TAG, "SchedulerRapidCancellationComponent setup"); }

void SchedulerRapidCancellationComponent::run_rapid_cancellation_test() {
  ESP_LOGI(TAG, "Starting rapid cancellation test - multiple threads racing on same timeout names");

  // Reset counters
  this->total_scheduled_ = 0;
  this->total_executed_ = 0;

  static constexpr int NUM_THREADS = 4;              // Number of threads to create
  static constexpr int NUM_NAMES = 10;               // Only 10 unique names
  static constexpr int OPERATIONS_PER_THREAD = 100;  // Each thread does 100 operations

  // Create threads that will all fight over the same timeout names
  std::vector<std::thread> threads;
  threads.reserve(NUM_THREADS);

  for (int thread_id = 0; thread_id < NUM_THREADS; thread_id++) {
    threads.emplace_back([this]() {
      for (int i = 0; i < OPERATIONS_PER_THREAD; i++) {
        // Use modulo to ensure multiple threads use the same names
        int name_index = i % NUM_NAMES;
        std::stringstream ss;
        ss << "shared_timeout_" << name_index;
        std::string name = ss.str();

        // All threads schedule timeouts - this will implicitly cancel existing ones
        this->set_timeout(name, 150, [this, name]() {
          this->total_executed_.fetch_add(1);
          ESP_LOGI(TAG, "Executed callback '%s'", name.c_str());
        });
        this->total_scheduled_.fetch_add(1);

        // Small delay to increase chance of race conditions
        if (i % 10 == 0) {
          std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
      }
    });
  }

  // Wait for all threads to complete
  for (auto &t : threads) {
    t.join();
  }

  ESP_LOGI(TAG, "All threads completed. Scheduled: %d", this->total_scheduled_.load());

  // Give some time for any remaining callbacks to execute
  this->set_timeout("final_timeout", 200, [this]() {
    ESP_LOGI(TAG, "Rapid cancellation test complete. Final stats:");
    ESP_LOGI(TAG, "  Total scheduled: %d", this->total_scheduled_.load());
    ESP_LOGI(TAG, "  Total executed: %d", this->total_executed_.load());

    // Calculate implicit cancellations (timeouts replaced when scheduling same name)
    int implicit_cancellations = this->total_scheduled_.load() - this->total_executed_.load();
    ESP_LOGI(TAG, "  Implicit cancellations (replaced): %d", implicit_cancellations);
    ESP_LOGI(TAG, "  Total accounted: %d (executed + implicit cancellations)",
             this->total_executed_.load() + implicit_cancellations);

    // Final message to signal test completion - ensures all stats are logged before test ends
    ESP_LOGI(TAG, "Test finished - all statistics reported");
  });
}

}  // namespace scheduler_rapid_cancellation_component
}  // namespace esphome
