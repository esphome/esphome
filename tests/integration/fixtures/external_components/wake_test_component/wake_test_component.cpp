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

}  // namespace esphome::wake_test_component
