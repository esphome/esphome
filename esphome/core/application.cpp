#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"
#include "esphome/core/hal.h"

#ifdef USE_STATUS_LED
#include "esphome/components/status_led/status_led.h"
#endif

namespace esphome {

static const char *const TAG = "app";

void Application::register_component_(Component *comp) {
  if (comp == nullptr) {
    ESP_LOGW(TAG, "Tried to register null component!");
    return;
  }

  for (auto *c : this->components_) {
    if (comp == c) {
      ESP_LOGW(TAG, "Component %s already registered! (%p)", c->get_component_source(), c);
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

    // Update loop_component_start_time_ before calling each component during setup
    this->loop_component_start_time_ = millis();
    component->call();
    this->scheduler.process_to_add();
    this->feed_wdt();
    if (component->can_proceed())
      continue;

    std::stable_sort(this->components_.begin(), this->components_.begin() + i + 1,
                     [](Component *a, Component *b) { return a->get_loop_priority() > b->get_loop_priority(); });

    do {
      uint32_t new_app_state = STATUS_LED_WARNING;
      this->scheduler.call();
      this->feed_wdt();
      for (uint32_t j = 0; j <= i; j++) {
        // Update loop_component_start_time_ right before calling each component
        this->loop_component_start_time_ = millis();
        this->components_[j]->call();
        new_app_state |= this->components_[j]->get_component_state();
        this->app_state_ |= new_app_state;
        this->feed_wdt();
      }
      this->app_state_ = new_app_state;
      yield();
    } while (!component->can_proceed());
  }

  ESP_LOGI(TAG, "setup() finished successfully!");
  this->schedule_dump_config();
  this->calculate_looping_components_();
}
void Application::loop() {
  uint32_t new_app_state = 0;

  this->scheduler.call();

  // Get the initial loop time at the start
  uint32_t last_op_end_time = millis();

  // Feed WDT with time
  this->feed_wdt(last_op_end_time);

  for (Component *component : this->looping_components_) {
    // Update the cached time before each component runs
    this->loop_component_start_time_ = last_op_end_time;

    {
      this->set_current_component(component);
      WarnIfComponentBlockingGuard guard{component, last_op_end_time};
      component->call();
      // Use the finish method to get the current time as the end time
      last_op_end_time = guard.finish();
    }
    new_app_state |= component->get_component_state();
    this->app_state_ |= new_app_state;
    this->feed_wdt(last_op_end_time);
  }
  this->app_state_ = new_app_state;

  // Use the last component's end time instead of calling millis() again
  auto elapsed = last_op_end_time - this->last_loop_;
  if (elapsed >= this->loop_interval_ || HighFrequencyLoopRequester::is_high_frequency()) {
    yield();
  } else {
    uint32_t delay_time = this->loop_interval_ - elapsed;
    uint32_t next_schedule = this->scheduler.next_schedule_in().value_or(delay_time);
    // next_schedule is max 0.5*delay_time
    // otherwise interval=0 schedules result in constant looping with almost no sleep
    next_schedule = std::max(next_schedule, delay_time / 2);
    delay_time = std::min(next_schedule, delay_time);
    delay(delay_time);
  }
  this->last_loop_ = last_op_end_time;

  if (this->dump_config_at_ < this->components_.size()) {
    if (this->dump_config_at_ == 0) {
      ESP_LOGI(TAG, "ESPHome version " ESPHOME_VERSION " compiled on %s", this->compilation_time_);
#ifdef ESPHOME_PROJECT_NAME
      ESP_LOGI(TAG, "Project " ESPHOME_PROJECT_NAME " version " ESPHOME_PROJECT_VERSION);
#endif
    }

    this->components_[this->dump_config_at_]->call_dump_config();
    this->dump_config_at_++;
  }
}

void IRAM_ATTR HOT Application::feed_wdt(uint32_t time) {
  static uint32_t last_feed = 0;
  // Use provided time if available, otherwise get current time
  uint32_t now = time ? time : millis();
  // Compare in milliseconds (3ms threshold)
  if (now - last_feed > 3) {
    arch_feed_wdt();
    last_feed = now;
#ifdef USE_STATUS_LED
    if (status_led::global_status_led != nullptr) {
      status_led::global_status_led->call();
    }
#endif
  }
}
void Application::reboot() {
  ESP_LOGI(TAG, "Forcing a reboot...");
  for (auto it = this->components_.rbegin(); it != this->components_.rend(); ++it) {
    (*it)->on_shutdown();
  }
  arch_restart();
}
void Application::safe_reboot() {
  ESP_LOGI(TAG, "Rebooting safely...");
  run_safe_shutdown_hooks();
  arch_restart();
}

void Application::run_safe_shutdown_hooks() {
  for (auto it = this->components_.rbegin(); it != this->components_.rend(); ++it) {
    (*it)->on_safe_shutdown();
  }
  for (auto it = this->components_.rbegin(); it != this->components_.rend(); ++it) {
    (*it)->on_shutdown();
  }
}

void Application::calculate_looping_components_() {
  for (auto *obj : this->components_) {
    if (obj->has_overridden_loop())
      this->looping_components_.push_back(obj);
  }
}

Application App;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome
