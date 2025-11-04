#include "defer_stress_component.h"
#include "esphome/core/log.h"
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>

namespace esphome {
namespace defer_stress_component {

static const char *const TAG = "defer_stress";

void DeferStressComponent::setup() { ESP_LOGCONFIG(TAG, "DeferStressComponent setup"); }

void DeferStressComponent::run_multi_thread_test() {
  // Use member variables instead of static to avoid issues
  this->total_defers_ = 0;
  this->executed_defers_ = 0;
  static constexpr int NUM_THREADS = 10;
  static constexpr int DEFERS_PER_THREAD = 100;

  ESP_LOGI(TAG, "Starting defer stress test - multi-threaded concurrent defers");

  // Ensure we're starting clean
  ESP_LOGI(TAG, "Initial counters: total=%d, executed=%d", this->total_defers_.load(), this->executed_defers_.load());

  // Track start time
  auto start_time = std::chrono::steady_clock::now();

  // Create threads
  std::vector<std::thread> threads;

  ESP_LOGI(TAG, "Creating %d threads, each will defer %d callbacks", NUM_THREADS, DEFERS_PER_THREAD);

  threads.reserve(NUM_THREADS);
  for (int i = 0; i < NUM_THREADS; i++) {
    threads.emplace_back([this, i]() {
      ESP_LOGV(TAG, "Thread %d starting", i);
      // Each thread directly calls defer() without any locking
      for (int j = 0; j < DEFERS_PER_THREAD; j++) {
        int defer_id = this->total_defers_.fetch_add(1);
        ESP_LOGV(TAG, "Thread %d calling defer for request %d", i, defer_id);

        // Capture this pointer safely for the lambda
        auto *component = this;

        // Directly call defer() from this thread - no locking!
        this->defer([component, i, j, defer_id]() {
          component->executed_defers_.fetch_add(1);
          ESP_LOGV(TAG, "Executed defer %d (thread %d, index %d)", defer_id, i, j);
        });

        ESP_LOGV(TAG, "Thread %d called defer for request %d successfully", i, defer_id);

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
  ESP_LOGI(TAG, "All threads finished in %lldms. Created %d defer requests", thread_time, this->total_defers_.load());
}

}  // namespace defer_stress_component
}  // namespace esphome
