#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"

#ifdef USE_STATUS_LED
#include "esphome/components/status_led/status_led.h"
#endif

namespace esphome {

static const char *TAG = "app";

void Application::register_component_(Component *comp) {
  if (comp == nullptr) {
    ESP_LOGW(TAG, "Tried to register null component!");
    return;
  }

  for (auto *c : this->components_) {
    if (comp == c) {
      ESP_LOGW(TAG, "Component already registered! (%p)", c);
      return;
    }
  }
  this->components_.push_back(comp);
}
void Application::setup() {
  ESP_LOGI(TAG, "Running through setup()...");
  ESP_LOGV(TAG, "Sorting components by setup priority...");
  std::stable_sort(this->components_.begin(), this->components_.end(), [](const Component *a, const Component *b) {
    return a->get_actual_setup_priority() > b->get_actual_setup_priority();
  });

  for (uint32_t i = 0; i < this->components_.size(); i++) {
    Component *component = this->components_[i];

    component->call();
    this->scheduler.process_to_add();
    if (component->can_proceed())
      continue;

    std::stable_sort(this->components_.begin(), this->components_.begin() + i + 1,
                     [](Component *a, Component *b) { return a->get_loop_priority() > b->get_loop_priority(); });

    do {
      uint32_t new_app_state = STATUS_LED_WARNING;
      this->scheduler.call();
      for (uint32_t j = 0; j <= i; j++) {
        this->components_[j]->call();
        new_app_state |= this->components_[j]->get_component_state();
        this->app_state_ |= new_app_state;
      }
      this->app_state_ = new_app_state;
      yield();
    } while (!component->can_proceed());
  }

  ESP_LOGI(TAG, "setup() finished successfully!");
  this->schedule_dump_config();
}
void Application::loop() {
  uint32_t new_app_state = 0;
  const uint32_t start = millis();

  this->scheduler.call();
  for (Component *component : this->components_) {
    component->call();
    new_app_state |= component->get_component_state();
    this->app_state_ |= new_app_state;
    this->feed_wdt();
  }
  this->app_state_ = new_app_state;

  const uint32_t end = millis();
  if (end - start > 200) {
    ESP_LOGV(TAG, "A component took a long time in a loop() cycle (%.2f s).", (end - start) / 1e3f);
    ESP_LOGV(TAG, "Components should block for at most 20-30ms in loop().");
  }

  const uint32_t now = millis();

  if (HighFrequencyLoopRequester::is_high_frequency()) {
    yield();
  } else {
    uint32_t delay_time = this->loop_interval_;
    if (now - this->last_loop_ < this->loop_interval_)
      delay_time = this->loop_interval_ - (now - this->last_loop_);

    uint32_t next_schedule = this->scheduler.next_schedule_in().value_or(delay_time);
    // next_schedule is max 0.5*delay_time
    // otherwise interval=0 schedules result in constant looping with almost no sleep
    next_schedule = std::max(next_schedule, delay_time / 2);
    delay_time = std::min(next_schedule, delay_time);
    delay(delay_time);
  }
  this->last_loop_ = now;

  if (this->dump_config_at_ >= 0 && this->dump_config_at_ < this->components_.size()) {
    if (this->dump_config_at_ == 0) {
      ESP_LOGI(TAG, "esphome version " ESPHOME_VERSION " compiled on %s", this->compilation_time_.c_str());
    }

    this->components_[this->dump_config_at_]->dump_config();
    this->dump_config_at_++;
  }
}

void ICACHE_RAM_ATTR HOT Application::feed_wdt() {
  static uint32_t LAST_FEED = 0;
  uint32_t now = millis();
  if (now - LAST_FEED > 3) {
#ifdef ARDUINO_ARCH_ESP8266
    ESP.wdtFeed();
#endif
#ifdef ARDUINO_ARCH_ESP32
    yield();
#endif
    LAST_FEED = now;
#ifdef USE_STATUS_LED
    if (status_led::global_status_led != nullptr) {
      status_led::global_status_led->call();
    }
#endif
  }
}
void Application::reboot() {
  ESP_LOGI(TAG, "Forcing a reboot...");
  for (auto *comp : this->components_)
    comp->on_shutdown();
  ESP.restart();
  // restart() doesn't always end execution
  while (true) {
    yield();
  }
}
void Application::safe_reboot() {
  ESP_LOGI(TAG, "Rebooting safely...");
  for (auto *comp : this->components_)
    comp->on_safe_shutdown();
  for (auto *comp : this->components_)
    comp->on_shutdown();
  ESP.restart();
  // restart() doesn't always end execution
  while (true) {
    yield();
  }
}

Application App;

}  // namespace esphome
