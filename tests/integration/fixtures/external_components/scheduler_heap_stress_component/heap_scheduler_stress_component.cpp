#include "heap_scheduler_stress_component.h"
#include "esphome/core/log.h"
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <random>

namespace esphome {
namespace scheduler_heap_stress_component {

static const char *const TAG = "scheduler_heap_stress";

void SchedulerHeapStressComponent::setup() { ESP_LOGCONFIG(TAG, "SchedulerHeapStressComponent setup"); }

void SchedulerHeapStressComponent::run_multi_thread_test() {
  // Use member variables instead of static to avoid issues
  this->total_callbacks_ = 0;
  this->executed_callbacks_ = 0;
  static constexpr int NUM_THREADS = 10;
  static constexpr int CALLBACKS_PER_THREAD = 100;

  ESP_LOGI(TAG, "Starting heap scheduler stress test - multi-threaded concurrent set_timeout/set_interval");

  // Ensure we're starting clean
  ESP_LOGI(TAG, "Initial counters: total=%d, executed=%d", this->total_callbacks_.load(),
           this->executed_callbacks_.load());

  // Track start time
  auto start_time = std::chrono::steady_clock::now();

  // Create threads
  std::vector<std::thread> threads;

  ESP_LOGI(TAG, "Creating %d threads, each will schedule %d callbacks", NUM_THREADS, CALLBACKS_PER_THREAD);

  threads.reserve(NUM_THREADS);
  for (int i = 0; i < NUM_THREADS; i++) {
    threads.emplace_back([this, i]() {
      ESP_LOGV(TAG, "Thread %d starting", i);

      // Random number generator for this thread
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> timeout_dist(1, 100);    // 1-100ms timeouts
      std::uniform_int_distribution<> interval_dist(10, 200);  // 10-200ms intervals
      std::uniform_int_distribution<> type_dist(0, 1);         // 0=timeout, 1=interval

      // Each thread directly calls set_timeout/set_interval without any locking
      for (int j = 0; j < CALLBACKS_PER_THREAD; j++) {
        int callback_id = this->total_callbacks_.fetch_add(1);
        bool use_interval = (type_dist(gen) == 1);

        ESP_LOGV(TAG, "Thread %d scheduling %s for callback %d", i, use_interval ? "interval" : "timeout", callback_id);

        // Capture this pointer safely for the lambda
        auto *component = this;

        if (use_interval) {
          // Use set_interval with random interval time
          uint32_t interval_ms = interval_dist(gen);

          this->set_interval(interval_ms, [component, i, j, callback_id]() {
            component->executed_callbacks_.fetch_add(1);
            ESP_LOGV(TAG, "Executed interval %d (thread %d, index %d)", callback_id, i, j);

            // Cancel the interval after first execution to avoid flooding
            return false;
          });

          ESP_LOGV(TAG, "Thread %d scheduled interval %d with %u ms interval", i, callback_id, interval_ms);
        } else {
          // Use set_timeout with random timeout
          uint32_t timeout_ms = timeout_dist(gen);

          this->set_timeout(timeout_ms, [component, i, j, callback_id]() {
            component->executed_callbacks_.fetch_add(1);
            ESP_LOGV(TAG, "Executed timeout %d (thread %d, index %d)", callback_id, i, j);
          });

          ESP_LOGV(TAG, "Thread %d scheduled timeout %d with %u ms delay", i, callback_id, timeout_ms);
        }

        // Small random delay to increase contention
        if (j % 10 == 0) {
          std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
      }
      ESP_LOGV(TAG, "Thread %d finished", i);
    });
  }

  // Wait for all threads to complete
  for (auto &t : threads) {
    t.join();
  }

  auto end_time = std::chrono::steady_clock::now();
  auto thread_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
  ESP_LOGI(TAG, "All threads finished in %lldms. Created %d callbacks", thread_time, this->total_callbacks_.load());
}

}  // namespace scheduler_heap_stress_component
}  // namespace esphome
