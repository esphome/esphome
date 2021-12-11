#include "esphome/core/component.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <utility>

namespace esphome {

static const char *const TAG = "component";

namespace setup_priority {

const float BUS = 1000.0f;
const float IO = 900.0f;
const float HARDWARE = 800.0f;
const float DATA = 600.0f;
const float PROCESSOR = 400.0;
const float BLUETOOTH = 350.0f;
const float AFTER_BLUETOOTH = 300.0f;
const float WIFI = 250.0f;
const float BEFORE_CONNECTION = 220.0f;
const float AFTER_WIFI = 200.0f;
const float AFTER_CONNECTION = 100.0f;
const float LATE = -100.0f;

}  // namespace setup_priority

const uint32_t COMPONENT_STATE_MASK = 0xFF;
const uint32_t COMPONENT_STATE_CONSTRUCTION = 0x00;
const uint32_t COMPONENT_STATE_SETUP = 0x01;
const uint32_t COMPONENT_STATE_LOOP = 0x02;
const uint32_t COMPONENT_STATE_FAILED = 0x03;
const uint32_t STATUS_LED_MASK = 0xFF00;
const uint32_t STATUS_LED_OK = 0x0000;
const uint32_t STATUS_LED_WARNING = 0x0100;
const uint32_t STATUS_LED_ERROR = 0x0200;

uint32_t global_state = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

float Component::get_loop_priority() const { return 0.0f; }

float Component::get_setup_priority() const { return setup_priority::DATA; }

void Component::setup() {}

void Component::loop() {}

void Component::set_interval(const std::string &name, uint32_t interval, std::function<void()> &&f) {  // NOLINT
  App.scheduler.set_interval(this, name, interval, std::move(f));
}

bool Component::cancel_interval(const std::string &name) {  // NOLINT
  return App.scheduler.cancel_interval(this, name);
}

void Component::set_retry(const std::string &name, uint32_t initial_wait_time, uint8_t max_attempts,
                          std::function<RetryResult()> &&f, float backoff_increase_factor) {  // NOLINT
  App.scheduler.set_retry(this, name, initial_wait_time, max_attempts, std::move(f), backoff_increase_factor);
}

bool Component::cancel_retry(const std::string &name) {  // NOLINT
  return App.scheduler.cancel_retry(this, name);
}

void Component::set_timeout(const std::string &name, uint32_t timeout, std::function<void()> &&f) {  // NOLINT
  return App.scheduler.set_timeout(this, name, timeout, std::move(f));
}

bool Component::cancel_timeout(const std::string &name) {  // NOLINT
  return App.scheduler.cancel_timeout(this, name);
}

void Component::call_loop() { this->loop(); }
void Component::call_setup() { this->setup(); }
void Component::call_dump_config() { this->dump_config(); }

uint32_t Component::get_component_state() const { return this->component_state_; }
void Component::call() {
  uint32_t state = this->component_state_ & COMPONENT_STATE_MASK;
  switch (state) {
    case COMPONENT_STATE_CONSTRUCTION:
      // State Construction: Call setup and set state to setup
      this->component_state_ &= ~COMPONENT_STATE_MASK;
      this->component_state_ |= COMPONENT_STATE_SETUP;
      this->call_setup();
      break;
    case COMPONENT_STATE_SETUP:
      // State setup: Call first loop and set state to loop
      this->component_state_ &= ~COMPONENT_STATE_MASK;
      this->component_state_ |= COMPONENT_STATE_LOOP;
      this->call_loop();
      break;
    case COMPONENT_STATE_LOOP:
      // State loop: Call loop
      this->call_loop();
      break;
    case COMPONENT_STATE_FAILED:  // NOLINT(bugprone-branch-clone)
      // State failed: Do nothing
      break;
    default:
      break;
  }
}
const char *Component::get_component_source() const {
  if (this->component_source_ == nullptr)
    return "<unknown>";
  return this->component_source_;
}
void Component::mark_failed() {
  ESP_LOGE(TAG, "Component %s was marked as failed.", this->get_component_source());
  this->component_state_ &= ~COMPONENT_STATE_MASK;
  this->component_state_ |= COMPONENT_STATE_FAILED;
  this->status_set_error();
}
void Component::defer(std::function<void()> &&f) {  // NOLINT
  App.scheduler.set_timeout(this, "", 0, std::move(f));
}
bool Component::cancel_defer(const std::string &name) {  // NOLINT
  return App.scheduler.cancel_timeout(this, name);
}
void Component::defer(const std::string &name, std::function<void()> &&f) {  // NOLINT
  App.scheduler.set_timeout(this, name, 0, std::move(f));
}
void Component::set_timeout(uint32_t timeout, std::function<void()> &&f) {  // NOLINT
  App.scheduler.set_timeout(this, "", timeout, std::move(f));
}
void Component::set_interval(uint32_t interval, std::function<void()> &&f) {  // NOLINT
  App.scheduler.set_interval(this, "", interval, std::move(f));
}
void Component::set_retry(uint32_t initial_wait_time, uint8_t max_attempts, std::function<RetryResult()> &&f,
                          float backoff_increase_factor) {  // NOLINT
  App.scheduler.set_retry(this, "", initial_wait_time, max_attempts, std::move(f), backoff_increase_factor);
}
bool Component::is_failed() { return (this->component_state_ & COMPONENT_STATE_MASK) == COMPONENT_STATE_FAILED; }
bool Component::can_proceed() { return true; }
bool Component::status_has_warning() { return this->component_state_ & STATUS_LED_WARNING; }
bool Component::status_has_error() { return this->component_state_ & STATUS_LED_ERROR; }
void Component::status_set_warning() {
  this->component_state_ |= STATUS_LED_WARNING;
  App.app_state_ |= STATUS_LED_WARNING;
}
void Component::status_set_error() {
  this->component_state_ |= STATUS_LED_ERROR;
  App.app_state_ |= STATUS_LED_ERROR;
}
void Component::status_clear_warning() { this->component_state_ &= ~STATUS_LED_WARNING; }
void Component::status_clear_error() { this->component_state_ &= ~STATUS_LED_ERROR; }
void Component::status_momentary_warning(const std::string &name, uint32_t length) {
  this->status_set_warning();
  this->set_timeout(name, length, [this]() { this->status_clear_warning(); });
}
void Component::status_momentary_error(const std::string &name, uint32_t length) {
  this->status_set_error();
  this->set_timeout(name, length, [this]() { this->status_clear_error(); });
}
void Component::dump_config() {}
float Component::get_actual_setup_priority() const {
  if (std::isnan(this->setup_priority_override_))
    return this->get_setup_priority();
  return this->setup_priority_override_;
}
void Component::set_setup_priority(float priority) { this->setup_priority_override_ = priority; }

bool Component::has_overridden_loop() const {
#ifdef CLANG_TIDY
  bool loop_overridden = true;
  bool call_loop_overridden = true;
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpmf-conversions"
  bool loop_overridden = (void *) (this->*(&Component::loop)) != (void *) (&Component::loop);
  bool call_loop_overridden = (void *) (this->*(&Component::call_loop)) != (void *) (&Component::call_loop);
#pragma GCC diagnostic pop
#endif
  return loop_overridden || call_loop_overridden;
}

PollingComponent::PollingComponent(uint32_t update_interval) : Component(), update_interval_(update_interval) {}

void PollingComponent::call_setup() {
  // Let the polling component subclass setup their HW.
  this->setup();

  // Register interval.
  this->set_interval("update", this->get_update_interval(), [this]() { this->update(); });
}

uint32_t PollingComponent::get_update_interval() const { return this->update_interval_; }
void PollingComponent::set_update_interval(uint32_t update_interval) { this->update_interval_ = update_interval; }

WarnIfComponentBlockingGuard::WarnIfComponentBlockingGuard(Component *component)
    : started_(millis()), component_(component) {}
WarnIfComponentBlockingGuard::~WarnIfComponentBlockingGuard() {
  uint32_t now = millis();
  if (now - started_ > 50) {
    const char *src = component_ == nullptr ? "<null>" : component_->get_component_source();
    ESP_LOGV(TAG, "Component %s took a long time for an operation (%.2f s).", src, (now - started_) / 1e3f);
    ESP_LOGV(TAG, "Components should block for at most 20-30ms.");
    ;
  }
}

}  // namespace esphome
