#include "simultaneous_callbacks_component.h"
#include "esphome/core/log.h"
#include <thread>
#include <vector>
#include <chrono>
#include <sstream>

namespace esphome {
namespace scheduler_simultaneous_callbacks_component {

static const char *const TAG = "scheduler_simultaneous_callbacks";

void SchedulerSimultaneousCallbacksComponent::setup() {
  ESP_LOGCONFIG(TAG, "SchedulerSimultaneousCallbacksComponent setup");
}

void SchedulerSimultaneousCallbacksComponent::run_simultaneous_callbacks_test() {
  ESP_LOGI(TAG, "Starting simultaneous callbacks test - 10 threads scheduling 100 callbacks each for 1ms from now");

  // Reset counters
  this->total_scheduled_ = 0;
  this->total_executed_ = 0;
  this->callbacks_at_once_ = 0;
  this->max_concurrent_ = 0;

  static constexpr int NUM_THREADS = 10;
  static constexpr int CALLBACKS_PER_THREAD = 100;
  static constexpr uint32_t DELAY_MS = 1;  // All callbacks scheduled for 1ms from now

  // Create threads for concurrent scheduling
  std::vector<std::thread> threads;
  threads.reserve(NUM_THREADS);

  // Record start time for synchronization
  auto start_time = std::chrono::steady_clock::now();

  for (int thread_id = 0; thread_id < NUM_THREADS; thread_id++) {
    threads.emplace_back([this, thread_id, start_time]() {
      ESP_LOGD(TAG, "Thread %d starting to schedule callbacks", thread_id);

      // Wait a tiny bit to ensure all threads start roughly together
      std::this_thread::sleep_until(start_time + std::chrono::microseconds(100));

      for (int i = 0; i < CALLBACKS_PER_THREAD; i++) {
        // Create unique name for each callback
        std::stringstream ss;
        ss << "thread_" << thread_id << "_cb_" << i;
        std::string name = ss.str();

        // Schedule callback for exactly DELAY_MS from now
        this->set_timeout(name, DELAY_MS, [this, name]() {
          // Increment concurrent counter atomically
          int current = this->callbacks_at_once_.fetch_add(1) + 1;

          // Update max concurrent if needed
          int expected = this->max_concurrent_.load();
          while (current > expected && !this->max_concurrent_.compare_exchange_weak(expected, current)) {
            // Loop until we successfully update or someone else set a higher value
          }

          ESP_LOGV(TAG, "Callback executed: %s (concurrent: %d)", name.c_str(), current);

          // Simulate some minimal work
          std::atomic<int> work{0};
          for (int j = 0; j < 10; j++) {
            work.fetch_add(j);
          }

          // Increment executed counter
          this->total_executed_.fetch_add(1);

          // Decrement concurrent counter
          this->callbacks_at_once_.fetch_sub(1);
        });

        this->total_scheduled_.fetch_add(1);
        ESP_LOGV(TAG, "Scheduled callback %s", name.c_str());
      }

      ESP_LOGD(TAG, "Thread %d completed scheduling", thread_id);
    });
  }

  // Wait for all threads to complete scheduling
  for (auto &t : threads) {
    t.join();
  }

  ESP_LOGI(TAG, "All threads completed scheduling. Total scheduled: %d", this->total_scheduled_.load());

  // Schedule a final timeout to check results after all callbacks should have executed
  this->set_timeout("final_check", 100, [this]() {
    ESP_LOGI(TAG, "Simultaneous callbacks test complete. Final executed count: %d", this->total_executed_.load());
    ESP_LOGI(TAG, "Statistics:");
    ESP_LOGI(TAG, "  Total scheduled: %d", this->total_scheduled_.load());
    ESP_LOGI(TAG, "  Total executed: %d", this->total_executed_.load());
    ESP_LOGI(TAG, "  Max concurrent callbacks: %d", this->max_concurrent_.load());

    if (this->total_executed_ == NUM_THREADS * CALLBACKS_PER_THREAD) {
      ESP_LOGI(TAG, "SUCCESS: All %d callbacks executed correctly!", this->total_executed_.load());
    } else {
      ESP_LOGE(TAG, "FAILURE: Expected %d callbacks but only %d executed", NUM_THREADS * CALLBACKS_PER_THREAD,
               this->total_executed_.load());
    }
  });
}

}  // namespace scheduler_simultaneous_callbacks_component
}  // namespace esphome
