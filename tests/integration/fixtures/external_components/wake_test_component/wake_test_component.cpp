#include "wake_test_component.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <chrono>
#include <thread>

namespace esphome::wake_test_component {

static const char *const TAG = "wake_test_component";

void WakeTestComponent::start_async_wake() {
  ESP_LOGI(TAG, "Spawning async wake thread (50ms delay)");
  std::thread([] {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    App.wake_loop_threadsafe();
  }).detach();
}

void WakeTestComponent::start_async_timeout() {
  const uint32_t start_time = millis();
  std::thread([this, start_time] {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    const int start_loop_count = this->get_loop_count();
    this->set_timeout("background-wake-test", 100, [this, start_time, start_loop_count] {
      ESP_LOGI(TAG, "SCHEDULER_WAKE_RESULT elapsed=%u loop_delta=%d", static_cast<unsigned int>(millis() - start_time),
               this->get_loop_count() - start_loop_count);
    });
  }).detach();
}

}  // namespace esphome::wake_test_component
