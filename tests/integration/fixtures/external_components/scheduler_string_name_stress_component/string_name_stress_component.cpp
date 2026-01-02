#include "string_name_stress_component.h"
#include "esphome/core/log.h"
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <string>
#include <sstream>

namespace esphome {
namespace scheduler_string_name_stress_component {

static const char *const TAG = "scheduler_string_name_stress";

void SchedulerStringNameStressComponent::setup() { ESP_LOGCONFIG(TAG, "SchedulerStringNameStressComponent setup"); }

void SchedulerStringNameStressComponent::run_string_name_stress_test() {
  // Use member variables to reset state
  this->total_callbacks_ = 0;
  this->executed_callbacks_ = 0;
  static constexpr int NUM_THREADS = 10;
  static constexpr int CALLBACKS_PER_THREAD = 100;

  ESP_LOGI(TAG, "Starting string name stress test - multi-threaded set_timeout with std::string names");
  ESP_LOGI(TAG, "This test specifically uses dynamic string names to test memory management");

  // Track start time
  auto start_time = std::chrono::steady_clock::now();

  // Create threads
  std::vector<std::thread> threads;

  ESP_LOGI(TAG, "Creating %d threads, each will schedule %d callbacks with dynamic names", NUM_THREADS,
           CALLBACKS_PER_THREAD);

  threads.reserve(NUM_THREADS);
  for (int i = 0; i < NUM_THREADS; i++) {
    threads.emplace_back([this, i]() {
      ESP_LOGV(TAG, "Thread %d starting", i);

      // Each thread schedules callbacks with dynamically created string names
      for (int j = 0; j < CALLBACKS_PER_THREAD; j++) {
        int callback_id = this->total_callbacks_.fetch_add(1);

        // Create a dynamic string name - this will test memory management
        std::stringstream ss;
        ss << "thread_" << i << "_callback_" << j << "_id_" << callback_id;
        std::string dynamic_name = ss.str();

        ESP_LOGV(TAG, "Thread %d scheduling timeout with dynamic name: %s", i, dynamic_name.c_str());

        // Capture necessary values for the lambda
        auto *component = this;

        // Schedule with std::string name - this tests the string overload
        // Use varying delays to stress the heap scheduler
        uint32_t delay = 1 + (callback_id % 50);

        // Also test nested scheduling from callbacks
        if (j % 10 == 0) {
          // Every 10th callback schedules another callback
          this->set_timeout(dynamic_name, delay, [component, callback_id]() {
            component->executed_callbacks_.fetch_add(1);
            ESP_LOGV(TAG, "Executed string-named callback %d (nested scheduler)", callback_id);

            // Schedule another timeout from within this callback with a new dynamic name
            std::string nested_name = "nested_from_" + std::to_string(callback_id);
            component->set_timeout(nested_name, 1, [callback_id]() {
              ESP_LOGV(TAG, "Executed nested string-named callback from %d", callback_id);
            });
          });
        } else {
          // Regular callback
          this->set_timeout(dynamic_name, delay, [component, callback_id]() {
            component->executed_callbacks_.fetch_add(1);
            ESP_LOGV(TAG, "Executed string-named callback %d", callback_id);
          });
        }

        // Add some timing variations to increase race conditions
        if (j % 5 == 0) {
          std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
      }
      ESP_LOGV(TAG, "Thread %d finished scheduling", i);
    });
  }

  // Wait for all threads to complete scheduling
  for (auto &t : threads) {
    t.join();
  }

  auto end_time = std::chrono::steady_clock::now();
  auto thread_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
  ESP_LOGI(TAG, "All threads finished scheduling in %lldms. Created %d callbacks with dynamic names", thread_time,
           this->total_callbacks_.load());

  // Give some time for callbacks to execute
  ESP_LOGI(TAG, "Waiting for callbacks to execute...");

  // Schedule a final callback to signal completion
  this->set_timeout("test_complete", 2000, [this]() {
    ESP_LOGI(TAG, "String name stress test complete. Executed %d of %d callbacks", this->executed_callbacks_.load(),
             this->total_callbacks_.load());
  });
}

}  // namespace scheduler_string_name_stress_component
}  // namespace esphome
